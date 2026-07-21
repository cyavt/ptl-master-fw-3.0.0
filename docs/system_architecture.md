# Tài Liệu Kiến Trúc Hệ Thống & Giao Thức Truyền Thông
**Dự án**: Smart Master (STM32F411CEU6 Blackpill + Ethernet W5500 + RS485 Modbus RTU)

Tài liệu này mô tả chi tiết kiến trúc phần mềm FreeRTOS, luồng khởi động vi điều khiển, cơ chế hàng đợi liên tác vụ tối ưu (lock-free) và đặc tả giao thức truyền thông trong hệ thống.

---

## 1. Thiết Lập Tác Vụ FreeRTOS & Độ Ưu Tiên (Task Priorities)

Để đảm bảo tính thời gian thực cho giao tiếp Serial Modbus và tính ổn định cho kết nối mạng TCP/IP, các tác vụ được cấu hình như sau:

| Tên Task | Hàm Thực Thi | Độ Ưu Tiên | Vai Trò & Cơ Chế Hoạt Động |
| :--- | :--- | :--- | :--- |
| **`TaskModbusMaster`** | `StartTaskModbusMaster` | **`osPriorityNormal`** | **Thời gian thực (Time-critical)**: Tác vụ ngầm của thư viện Modbus. Chuyên trách đóng gói khung truyền, gửi dữ liệu qua UART2 và đợi ngắt phản hồi từ Slave. Đa số thời gian ngủ chờ tín hiệu ngắt. |
| **`modbusTask`** | `StartModbusAppTask` | **`osPriorityNormal`** | **Tác vụ ứng dụng**: Quản lý logic quét Slave, kiểm tra kết nối định kỳ (heartbeat) và gửi lệnh cập nhật màn hình. |
| **`defaultTask`** | `StartDefaultTask` | **`osPriorityNormal`** | **Tác vụ mạng**: Quản lý kết nối Ethernet (W5500), kết nối lại MQTT Broker, gửi Ping giữ mạng (Keep-alive) định kỳ, nhận lệnh điều khiển màn hình và đẩy tin nhắn xếp hàng từ Modbus lên Server. |

---

## 2. Hàng Đợi Liên Tác Vụ (IPC Message Queue)
Nhằm loại bỏ hoàn toàn cơ chế khóa Mutex (`mqtt_mutex`) gây nguy cơ tắc nghẽn hoặc đảo ngược độ ưu tiên, hệ thống sử dụng một hàng đợi trung chuyển **`mqtt_tx_queueHandle`** (dung lượng 8 phần tử, cấu trúc `mqtt_msg_t`):

```c
typedef struct {
    char topic[64];   // Chủ đề gửi MQTT
    char *payload;    // Con trỏ vùng nhớ chứa chuỗi JSON (Cấp phát động bằng pvPortMalloc)
} mqtt_msg_t;
```

### Luồng Hoạt Động của Hàng Đợi:
```mermaid
graph TD
    subgraph modbusTask (Tác vụ Modbus)
        A[Quét thiết bị / Nhận ACK] --> B[Cấp phát RAM cho chuỗi JSON]
        B --> C[Đẩy tin nhắn vào Queue]
    end
    
    subgraph mqtt_tx_queueHandle (Hàng đợi liên tác vụ)
        C --> D[Xếp hàng 68 bytes/tin]
    end
    
    subgraph defaultTask (Tác vụ mạng)
        D --> E[Lấy tin nhắn ra]
        E --> F[Gửi qua chip mạng W5500]
        F --> G[Giải phóng bộ nhớ RAM]
    end
```

---

## 3. Giao Thức Giao Tiếp Mạng (Server <--> Master)

Mạch Master giao tiếp với MQTT Broker thông qua các Topic có dạng `master/<UID>/...` với `<UID>` là mã định danh phần cứng duy nhất của chip (ví dụ: `0068002B3030510A36313837`).

### 3.1. Lệnh Điều Khiển Màn Hình (Server --> Master)
* **Topic**: `master/<UID>/display/set`
* **Định dạng**: JSON
```json
{
  "id": 5,          // ID Slave mục tiêu (1-130). ID = 0 là Broadcast toàn bộ.
  "val": "12.34",   // Giá trị hiển thị (Chữ ASCII hoặc Số). [Tùy chọn]
  "intensity": 12,  // Độ sáng màn hình (0-15). [Tùy chọn]
  "state": 1,       // Trạng thái màn hình (0: Tắt, 1: Bật). [Tùy chọn]
  "pad": 0          // Chế độ đệm số 0 (0: Tắt, 1: Bật). [Tùy chọn]
}
```

### 3.2. Phản Hồi Kết Quả Ghi Lệnh (Master --> Server)
* **Topic**: `master/<UID>/display/status`
* **Định dạng**: JSON
  * **Thành công (Dạng chữ)**: `{"status":"success","id":5,"val":"HELO","intensity":12,"state":1,"pad":0}`
  * **Thành công (Dạng số)**: `{"status":"success","id":5,"val":12.34,"intensity":12,"state":1,"pad":0}`
  * **Thất bại do Timeout**: `{"status":"failed","id":5,"error":"timeout"}`
  * **Thất bại do Thiết bị offline**: `{"status":"failed","id":5,"error":"not_online"}`

### 3.3. Báo Cáo Thiết Bị Online (Master --> Server)
* **Topic**: `master/<UID>/slaves/status`
* **Tần suất**: Gửi **Định kỳ mỗi 10 giây** để giữ trạng thái trên Server, hoặc gửi **Ngay lập tức** khi phát hiện danh sách Slave online có biến động.
* **Định dạng**: JSON
  * **Có thiết bị online**: `{"online_slaves":[2,5,12]}`
  * **Không có thiết bị**: `{"online_slaves":[]}`

---

## 4. Giao Thức Giao Tiếp Vật Lý (Master <--> Slave)

Master giao tiếp với các thiết bị hiển thị Slave qua đường RS485 (UART2, 9600 bps, Half-duplex) dùng chuẩn Modbus RTU.

### 4.1. Bản Đồ Thanh Ghi Thiết Bị (Register Map)
Tất cả các lệnh sử dụng địa chỉ cơ sở bắt đầu từ **`0`**, độ dài **`5 thanh ghi`**:

| Địa Chỉ Thanh Ghi | Vai Trò Dữ Liệu | Cách Đóng Gói Byte |
| :---: | :--- | :--- |
| **Register 0** | Giá trị hiển thị (Phần 1) | * **Chế độ ASCII**: Byte Cao = Ký tự 1, Byte Thấp = Ký tự 2.<br> * **Chế độ Số**: Chứa 16-bit cao của số 32-bit. |
| **Register 1** | Giá trị hiển thị (Phần 2) | * **Chế độ ASCII**: Byte Cao = Ký tự 3, Byte Thấp = Ký tự 4.<br> * **Chế độ Số**: Chứa 16-bit thấp của số 32-bit. |
| **Register 2** | Độ sáng màn hình | Số nguyên từ `0` đến `15`. |
| **Register 3** | Cấu hình chế độ & ký tự cuối | * **Chế độ ASCII**: Byte Cao = `0x01`, Byte Thấp = Ký tự 5.<br> * **Chế độ Số**: Byte Cao = `0x00`, Byte Thấp = Chế độ đệm số 0 (`0` hoặc `1`). |
| **Register 4** | Trạng thái hiển thị | `0` = Tắt màn hình, `1` = Bật màn hình. |

### 4.2. Các Lệnh Modbus RTU Thực Thi
* **Quét tìm / Giám sát (Heartbeat)**: Sử dụng mã chức năng **`FC03` (Read Holding Registers)** để đọc 5 thanh ghi từ địa chỉ `0`. Nếu Slave phản hồi đúng cấu trúc khung Modbus $\rightarrow$ Slave được xác định là Online.
* **Ghi lệnh hiển thị**: Sử dụng mã chức năng **`FC16` (Write Multiple Registers)** để ghi dữ liệu đóng gói xuống 5 thanh ghi bắt đầu từ địa chỉ `0`.

---

## 5. Luồng Hoạt Động Khởi Động (Startup Flow)

```
[ Cấp nguồn / Reset ]
         |
         v
[ Khởi tạo xung nhịp CPU 96MHz & Cấu hình ngoại vi GPIO, DMA, SPI1, UART1, UART2 ]
         |
         v
[ W5500_Init() : Reset cứng chip mạng, kiểm tra SPI, chờ cắm cáp mạng (PHY Link UP) ]
         |
         v
[ MQTT_App_Init() : Đọc mã Silicon Unique ID (UID), khởi tạo Client MQTT ]
         |
         v
[ Tạo các Mutex, Semaphore, Queue và 2 Tác vụ chính trong FreeRTOS ]
         |
         v
[ osKernelStart() : FreeRTOS nắm quyền điều khiển CPU ]
         +-----------------------------+-----------------------------+
         |                                                           |
         v                                                           v
[ defaultTask (Tác vụ mạng) ]                               [ modbusTask (Tác vụ Modbus) ]
         |                                                           |
[ Kết nối tới MQTT Broker ]                                 [ ModbusInit(): Tạo Task phụ, Timer ]
         |                                                           |
[ Chuyển cờ mqtt_is_connected = true ] <------------------- [ Chờ cờ mqtt_is_connected == true ]
         |                                                           |
[ Đón nhận lệnh hiển thị từ Server ]                        [ Chạy quét Startup Scan ID 1-130 ]
         |                                                           |
[ Giải phóng display_update_sem ] ------------------------> [ Nhận dạng Slave Online, lưu vào RAM ]
                                                                     |
                                                            [ Tắt màn hình Slave về trạng thái chờ ]
                                                                     |
                                                            [ Đẩy ACK trạng thái ban đầu vào Queue ]
                                                                     |
                                                            [ Vòng lặp tuần hoàn giám sát / Ghi lệnh ]
```

---

## 6. Tính Toán Thời Gian Quét (Scan Timing Estimates)

Chu kỳ thời gian giao tiếp được xác định bởi cấu hình:
* `MODBUS_FAST_TIMEOUT_MS` = **50 ms** (Thời gian chờ tối đa khi quét)
* `MODBUS_SCAN_YIELD_DELAY_MS` = **5 ms** (Thời gian nhường CPU giữa các thiết bị)

### 6.1. Thời gian quét 1 thiết bị (ID):
* **ID Offline**: Phải đợi hết Timeout + Delay nghỉ = **`55 ms`**.
* **ID Online**: Phản hồi thành công ngay lập tức + Delay nghỉ = **`20 ms` đến `25 ms`**.

### 6.2. Ước tính tổng thời gian quét đầy đủ (Full Scan):

| Số Lượng Slave | Tỷ lệ Thiết bị Online/Offline | Tổng thời gian quét ước tính |
| :---: | :--- | :---: |
| **100 Slave** | 100% Offline (Tắt hết) | **`5.5 giây`** |
| **100 Slave** | 100% Online (Bật hết) | **`2.3 giây`** |
| **100 Slave** | 30% Online / 70% Offline | **`4.5 giây`** |
| **130 Slave** | 100% Offline (Tắt hết) | **`7.15 giây`** |
| **130 Slave** | 10% Online / 90% Offline | **`6.8 giây`** |
