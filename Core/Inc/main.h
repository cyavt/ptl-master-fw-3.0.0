/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED13_Pin GPIO_PIN_13
#define LED13_GPIO_Port GPIOC
#define CS_Pin GPIO_PIN_4
#define CS_GPIO_Port GPIOA
#define RST_Pin GPIO_PIN_0
#define RST_GPIO_Port GPIOB
#define INT_Pin GPIO_PIN_1
#define INT_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_3
#define BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define MAX_SLAVE_DEVICES             50     // Số lượng thiết bị Slave tối đa hỗ trợ
#define MAX_SLAVE_ARRAY_SIZE          (MAX_SLAVE_DEVICES + 1) // Kích thước mảng cho ID Slave (từ 1 đến MAX)

#define MODBUS_STANDARD_TIMEOUT_MS    1000    // Thời gian chờ phản hồi Modbus tiêu chuẩn (ms)
#define MODBUS_FAST_TIMEOUT_MS        50      // Thời gian chờ phản hồi Modbus nhanh khi quét hoặc ping (ms)
#define MODBUS_TRANSACTION_DELAY_MS   20      // Độ trễ giữa các lượt truyền Modbus thông thường (ms)
#define MODBUS_SCAN_YIELD_DELAY_MS    5       // Độ trễ giữa các lượt quét ID để nhường thời gian CPU (ms)
#define MODBUS_RETRY_DELAY_MS         50      // Độ trễ trước khi gửi lại khi ghi lệnh lỗi (ms)

#define OFFLINE_CHECK_INTERVAL_MS     10000   // Chu kỳ quét ngầm để kiểm tra kết nối lại của các Slave offline (ms)
#define STATUS_PUBLISH_INTERVAL_MS    10000   // Chu kỳ gửi danh sách Slave online lên MQTT Server (ms)
#define HEARTBEAT_CONSECUTIVE_FAILS   3       // Số lần lỗi heartbeat liên tiếp trước khi xác nhận Slave offline


typedef struct {
    char text[6];          // 5 ký tự hiển thị + \0
    uint8_t intensity;     // Độ sáng (0-15)
    uint8_t padding;       // Chế độ đệm số 0
    uint8_t state;         // Trạng thái bật/tắt (0 = Tắt, 1 = Bật)
    uint8_t is_configured; // 1 = Đã có lịch sử cấu hình
    uint8_t is_ascii;      // 0 = Chế độ số, 1 = Chế độ chuỗi ASCII
    uint8_t pending_write; // 1 = Có lệnh ghi đang chờ xử lý
} slave_display_t;

typedef struct {
    char topic[64];
    char *payload;
} mqtt_msg_t;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
