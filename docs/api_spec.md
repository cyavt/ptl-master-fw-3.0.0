# MQTT API Specification (Đặc tả API Truyền Thông)
**Phiên bản**: 1.0.0  
**Đối tượng áp dụng**: Mạch Smart Master STM32F411

Tài liệu này đặc tả quy chuẩn giao tiếp thông tin qua giao thức MQTT giữa **Cloud Server** (hoặc Dashboard điều khiển) và thiết bị **Smart Master**.

---

## 1. Quy Tắc Đặt Tên Kênh Truyền (Topic Naming Rules)

Tất cả các Topic đều sử dụng tiền tố động chứa mã định danh Unique ID (`<UID>`) của vi điều khiển (24 ký tự Hex, ví dụ: `0068002B3030510A36313837`). Điều này giúp tránh trùng lặp khi chạy hàng nghìn thiết bị trên cùng một Broker.

* **UID** được định dạng từ 3 thanh ghi Unique Device ID của STM32:
  `sprintf(master_uid, "%08X%08X%08X", HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2())`

---

## 2. Các Kênh Đăng Ký Nhận Lệnh (Subscribed Topics)

### 2.1. Lệnh điều khiển màn hình Slave
* **Topic**: `master/<UID>/display/set`
* **QoS**: `QoS 1` (Giao hàng ít nhất một lần)
* **Định dạng dữ liệu**: JSON
* **Đặc tả các trường dữ liệu**:

| Trường | Kiểu | Bắt buộc | Mô tả |
| :--- | :--- | :---: | :--- |
| `id` | Integer | **Có** | ID của thiết bị hiển thị Slave (dải từ `1` đến `130`). Nếu `id = 0`, Master sẽ gửi phát quảng bá (Broadcast) lệnh này tới tất cả các Slave online trên bus RS485. |
| `val` | String / Int | Không | Nội dung cần hiển thị trên màn hình LED (tối đa 5 ký tự). Ví dụ: `"12.34"`, `"ERROR"`, `12000`. |
| `intensity`| Integer | Không | Độ sáng màn hình hiển thị. Dải giá trị từ `0` (Tối nhất) đến `15` (Sáng nhất). |
| `state` | Integer | Không | Trạng thái hiển thị màn hình: `0` (Tắt hiển thị), `1` (Bật hiển thị). |
| `pad` | Integer | Không | Chế độ điền số 0 phía trước (chỉ áp dụng cho số): `0` (Tắt, hiển thị `"   45"`), `1` (Bật, hiển thị `"00045"`). |

* **Ví dụ payload điều khiển Slave cụ thể (ID = 5)**:
  ```json
  {
    "id": 5,
    "val": "12.34",
    "intensity": 10,
    "state": 1,
    "pad": 0
  }
  ```
* **Ví dụ payload tắt hiển thị tất cả các màn hình (Broadcast)**:
  ```json
  {
    "id": 0,
    "state": 0
  }
  ```

---

## 3. Các Kênh Xuất Bản Báo Cáo (Published Topics)

### 3.1. Phản hồi trạng thái thực thi lệnh (ACK)
Mỗi khi nhận được lệnh điều khiển hiển thị từ kênh `display/set`, Master sẽ thực thi lệnh đó qua bus Modbus RS485 và lập tức phản hồi kết quả về Server trên kênh này.

* **Topic**: `master/<UID>/display/status`
* **QoS**: `QoS 0` hoặc `QoS 1`
* **Định dạng**: JSON
* **Các trường hợp phản hồi**:

#### A. Ghi lệnh thành công (Success)
Báo cáo trạng thái hiển thị hiện tại của Slave sau khi cập nhật thành công.
* *Dạng chữ (ASCII)*:
  ```json
  {
    "status": "success",
    "id": 5,
    "val": "HELO",
    "intensity": 10,
    "state": 1,
    "pad": 0
  }
  ```
* *Dạng số (Numeric)*:
  ```json
  {
    "status": "success",
    "id": 5,
    "val": 12.34,
    "intensity": 10,
    "state": 1,
    "pad": 0
  }
  ```

#### B. Thất bại do thiết bị không Online (Not Online)
Master kiểm tra danh sách trong RAM thấy Slave chưa từng online nên không gửi lệnh xuống RS485.
* *Payload*:
  ```json
  {
    "status": "failed",
    "id": 5,
    "error": "not_online"
  }
  ```

#### C. Thất bại do mất kết nối vật lý (Timeout)
Thiết bị có tên trong danh sách Online nhưng khi Master phát lệnh Modbus ghi dữ liệu thì không thấy phản hồi (quá 50ms chờ và thử lại).
* *Payload*:
  ```json
  {
    "status": "failed",
    "id": 5,
    "error": "timeout"
  }
  ```

---

### 3.2. Báo cáo danh sách thiết bị Online (Online Registry)
Master xuất bản danh sách các thiết bị Slave đang kết nối hoạt động trên bus RS485. Bản tin này được gửi **Định kỳ mỗi 10 giây** hoặc **Ngay lập tức khi phát hiện có biến động** (một Slave bị đứt dây/mất nguồn, hoặc một Slave mới cắm nguồn trở lại).

* **Topic**: `master/<UID>/slaves/status`
* **QoS**: `QoS 0`
* **Định dạng**: JSON
* **Ví dụ payload có thiết bị online**:
  ```json
  {
    "online_slaves": [1, 2, 5, 12, 24]
  }
  ```
* **Ví dụ payload không có thiết bị nào online**:
  ```json
  {
    "online_slaves": []
  }
  ```
