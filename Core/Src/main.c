/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "wizchip_port.h"
#include "mqtt.h"
#include "Modbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
  .name = "modbusTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for mqtt_mutex */
osMutexId_t mqtt_mutexHandle;
const osMutexAttr_t mqtt_mutex_attributes = {
  .name = "mqtt_mutex"
};
/* Definitions for display_update_sem */
osSemaphoreId_t display_update_semHandle;
const osSemaphoreAttr_t display_update_sem_attributes = {
  .name = "display_update_sem"
};
/* USER CODE BEGIN PV */
modbusHandler_t hmaster;

volatile uint32_t display_value = 0;
volatile uint16_t display_intensity = 15;
volatile uint16_t display_padding = 1; // Default padding enabled (shows 00000 instead of 0)
volatile uint16_t display_state = 1; // 1=ON, 0=OFF
volatile bool display_updated = false;
volatile bool config_mode = false;
volatile uint8_t display_target_id = 0; // 0 means broadcast to all online slaves
osMutexId_t mqtt_mutex;
osSemaphoreId_t display_update_sem;
osMessageQueueId_t mqtt_tx_queueHandle;

uint8_t online_slave_ids[MAX_SLAVE_DEVICES] = {0};
volatile int online_slave_count = 0;

uint8_t registered_slave_ids[MAX_SLAVE_DEVICES] = {0};
volatile int registered_slave_count = 0;

slave_display_t slave_registry[MAX_SLAVE_ARRAY_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void StartDefaultTask(void *argument);
void StartModbusAppTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (W5500_Init() != 0)
    Error_Handler();

  MQTT_App_Init();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of mqtt_mutex */
  mqtt_mutexHandle = osMutexNew(&mqtt_mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of display_update_sem */
  display_update_semHandle = osSemaphoreNew(1, 1, &display_update_sem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  mqtt_tx_queueHandle = osMessageQueueNew(32, sizeof(mqtt_msg_t), NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of modbusTask */
  modbusTaskHandle = osThreadNew(StartModbusAppTask, NULL, &modbusTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  mqtt_mutex = mqtt_mutexHandle;
  display_update_sem = display_update_semHandle;
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED13_GPIO_Port, LED13_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED13_Pin */
  GPIO_InitStruct.Pin = LED13_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED13_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RST_Pin */
  GPIO_InitStruct.Pin = RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : INT_Pin */
  GPIO_InitStruct.Pin = INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CONF_Pin */
  GPIO_InitStruct.Pin = CONF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(CONF_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    MQTT_App_Process();
    osDelay(10);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartModbusAppTask */
/**
* @brief Function implementing the modbusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartModbusAppTask */
void StartModbusAppTask(void *argument)
{
  /* USER CODE BEGIN StartModbusAppTask */
  static uint8_t online_slave_fail_count[MAX_SLAVE_ARRAY_SIZE] = {0};
  static uint32_t last_status_publish_tick = 0;

  // 1. Initialize Modbus Master
  printf("Modbus: Config USART2 RS485 as Modbus RTU Master, DMA mode.\r\n");
  
  hmaster.uModbusType = MB_MASTER;
  hmaster.port = &huart2;
  hmaster.u8id = 0;
  hmaster.EN_Port = NULL;
  hmaster.EN_Pin = 0;
  hmaster.u16timeOut = 1000;
  hmaster.xTypeHW = USART_HW_DMA;
  
  printf("Modbus: Initializing Modbus library...\r\n");
  ModbusInit(&hmaster);
  printf("Modbus: Starting Modbus task and timers...\r\n");
  ModbusStart(&hmaster);
  printf("Modbus: Master started successfully! Listening/transmitting on USART2 (RS485).\r\n");
  
  uint16_t read_data[5] = {0};
  modbus_t read_telegram;
  read_telegram.u8fct = MB_FC_READ_REGISTERS; // Read registers (FC03)
  read_telegram.u16RegAdd = 0;
  read_telegram.u16CoilsNo = 5;
  read_telegram.u16reg = read_data;

  uint16_t write_data[5] = {0};
  modbus_t write_telegram;
  write_telegram.u8fct = MB_FC_WRITE_MULTIPLE_REGISTERS; // Write registers (FC16)
  write_telegram.u16RegAdd = 0;
  write_telegram.u16CoilsNo = 5;
  write_telegram.u16reg = write_data;

  // --- Wait for MQTT Connection ---
  printf("Modbus: Waiting for MQTT connection to establish before running startup scan...\r\n");
  extern bool mqtt_is_connected;
  while (!mqtt_is_connected)
  {
    osDelay(100);
  }

  // --- Perform One-Time Startup Scan ---
  printf("Modbus: Running initial startup scan (ID 1-%d)...\r\n", MAX_SLAVE_DEVICES);
  xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_FAST_TIMEOUT_MS), 0);
  
  uint8_t startup_active_ids[MAX_SLAVE_DEVICES];
  int startup_active_count = 0;
  
  uint16_t scan_data[1];
  modbus_t scan_telegram;
  scan_telegram.u8fct = MB_FC_READ_REGISTERS; // Read 1 register
  scan_telegram.u16RegAdd = 0;
  scan_telegram.u16CoilsNo = 1;
  scan_telegram.u16reg = scan_data;
  
  for (int id = 1; id <= MAX_SLAVE_DEVICES; id++)
  {
    scan_telegram.u8id = id;
    uint32_t result = ModbusQueryV2(&hmaster, scan_telegram);
    if (result == OP_OK_QUERY)
    {
      printf("Modbus: Startup found active Slave ID %d\r\n", id);
      startup_active_ids[startup_active_count++] = id;
    }
    osDelay(MODBUS_SCAN_YIELD_DELAY_MS); // Yield to other tasks
  }
  
  xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_STANDARD_TIMEOUT_MS), 0);
  
  // Register found slaves
  online_slave_count = startup_active_count;
  memcpy(online_slave_ids, startup_active_ids, startup_active_count);
  registered_slave_count = startup_active_count;
  memcpy(registered_slave_ids, startup_active_ids, startup_active_count);
  printf("Modbus: Startup scan finished. Registered %d slaves.\r\n", online_slave_count);

  if (online_slave_count > 0)
  {
    printf("Modbus: Initializing display off - startup slaves...\r\n");
    for (int i = 0; i < online_slave_count; i++)
    {
      uint8_t sid = online_slave_ids[i];
      write_telegram.u8id = sid;
      write_data[0] = 0;
      write_data[1] = 0;
      write_data[2] = 15;
      write_data[3] = 1;
      write_data[4] = 0; // OFF
      ModbusQueryV2(&hmaster, write_telegram);
      
      // Update registry to reflect startup state (value 0, OFF)
      strcpy(slave_registry[sid].text, "0");
      slave_registry[sid].intensity = 15;
      slave_registry[sid].padding = 1;
      slave_registry[sid].state = 0;
      slave_registry[sid].is_configured = 1;
      slave_registry[sid].is_ascii = 0;
      osDelay(MODBUS_TRANSACTION_DELAY_MS);
    }
    
    publish_initial_slave_states();
  }

  // Trigger sync write to startup slaves (if any found)
  display_updated = false;
  // -------------------------------------

  /* Infinite loop */
  for(;;)
  {
    // 30-second offline slaves check
    uint32_t now_tick = HAL_GetTick();
    static uint32_t last_offline_check_tick = 0;
    if (!config_mode && (now_tick - last_offline_check_tick >= OFFLINE_CHECK_INTERVAL_MS || last_offline_check_tick == 0))
    {
      last_offline_check_tick = now_tick;
      bool online_list_changed = false;
      
      for (int i = 0; i < registered_slave_count; i++)
      {
        if (display_updated) break; // Break early if we have a pending display update command
        uint8_t registered_id = registered_slave_ids[i];
        
        // Check if registered_id is currently online
        bool is_currently_online = false;
        for (int j = 0; j < online_slave_count; j++)
        {
          if (online_slave_ids[j] == registered_id)
          {
            is_currently_online = true;
            break;
          }
        }
        
        if (!is_currently_online)
        {
          // Slave is registered but currently offline. Let's ping it!
          printf("Modbus: Pinging offline Slave ID %d to see if it came back...\r\n", registered_id);
          xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_FAST_TIMEOUT_MS), 0);
          
          read_telegram.u8id = registered_id;
          uint32_t result = ModbusQueryV2(&hmaster, read_telegram);
          
          xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_STANDARD_TIMEOUT_MS), 0);
          
          if (result == OP_OK_QUERY)
          {
            printf("Modbus: Offline Slave ID %d has come back online!\r\n", registered_id);
            online_slave_ids[online_slave_count++] = registered_id;
            online_slave_fail_count[registered_id] = 0;
            online_list_changed = true;
            slave_registry[registered_id].pending_write = 1; // Sync screen with registry value
            
            // Sort online_slave_ids to keep list in order
            for (int x = 0; x < online_slave_count - 1; x++) {
              for (int y = 0; y < online_slave_count - x - 1; y++) {
                if (online_slave_ids[y] > online_slave_ids[y+1]) {
                  uint8_t temp = online_slave_ids[y];
                  online_slave_ids[y] = online_slave_ids[y+1];
                  online_slave_ids[y+1] = temp;
                }
              }
            }
          }
          osDelay(MODBUS_SCAN_YIELD_DELAY_MS); // Yield briefly between pings
        }
      }
      
      if (online_list_changed)
      {
        // Publish updated list immediately to slaves/status
        extern bool mqtt_is_connected;
        if (mqtt_is_connected)
        {
          static char status_payload[1024];
          int pos = sprintf(status_payload, "{\"online_slaves\":[");
          for (int i = 0; i < online_slave_count; i++)
          {
            pos += sprintf(status_payload + pos, "%d", online_slave_ids[i]);
            if (i < online_slave_count - 1)
            {
              pos += sprintf(status_payload + pos, ",");
            }
          }
          sprintf(status_payload + pos, "]}");
          
          extern char master_uid[25];
          char status_topic[128];
          sprintf(status_topic, "master/%s/slaves/status", master_uid);
          MQTT_Queue_Publish(status_topic, status_payload);
          printf("MQTT: Queued updated online slaves list -> %s\r\n", status_payload);
          last_status_publish_tick = now_tick;
        }
        
        // Trigger screen write to synchronize the reconnected slave
        display_target_id = 0;
        display_updated = true;
      }
    }
    if (config_mode)
    {
      printf("Modbus: Config Mode active. Starting slave scan (ID 1-%d)...\r\n", MAX_SLAVE_DEVICES);
      
      // Temporarily change timeout to 150ms for faster scanning
      xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_FAST_TIMEOUT_MS), 0);
      
      uint8_t active_ids[MAX_SLAVE_DEVICES];
      int active_count = 0;
      
      uint16_t scan_data[1];
      modbus_t scan_telegram;
      scan_telegram.u8fct = MB_FC_READ_REGISTERS; // Read 1 register
      scan_telegram.u16RegAdd = 0;
      scan_telegram.u16CoilsNo = 1;
      scan_telegram.u16reg = scan_data;
      
      for (int id = 1; id <= MAX_SLAVE_DEVICES; id++)
      {
        if (!config_mode) // Check if user aborted config mode mid-scan
        {
          printf("Modbus: Config Mode disabled mid-scan. Aborting...\r\n");
          break;
        }
        
        scan_telegram.u8id = id;
        uint32_t result = ModbusQueryV2(&hmaster, scan_telegram);
        if (result == OP_OK_QUERY)
        {
          printf("Modbus: Found active Slave ID %d\r\n", id);
          active_ids[active_count++] = id;
        }
        osDelay(MODBUS_SCAN_YIELD_DELAY_MS); // Yield to other tasks
      }
      
      // Restore standard timeout of 1000ms
      xTimerChangePeriod(hmaster.xTimerTimeout, pdMS_TO_TICKS(MODBUS_STANDARD_TIMEOUT_MS), 0);
      
      if (config_mode)
      {
        // Copy temporary results to registry
        online_slave_count = active_count;
        memcpy(online_slave_ids, active_ids, active_count);
        registered_slave_count = active_count;
        memcpy(registered_slave_ids, active_ids, active_count);
        
        for (int i = 0; i < active_count; i++)
        {
          uint8_t id = active_ids[i];
          slave_registry[id].pending_write = 1;
        }
        
        // Construct JSON payload: {"online_slaves":[1, 5, 12]}
        static char payload[1024];
        int pos = sprintf(payload, "{\"online_slaves\":[");
        for (int i = 0; i < active_count; i++)
        {
          pos += sprintf(payload + pos, "%d", active_ids[i]);
          if (i < active_count - 1)
          {
            pos += sprintf(payload + pos, ",");
          }
        }
        sprintf(payload + pos, "]}");
        
        printf("Modbus: Scan finished. Results: %s\r\n", payload);
        
        // Publish to MQTT slaves/status topic directly
        extern bool mqtt_is_connected;
        if (mqtt_is_connected)
        {
          extern char master_uid[25];
          char topic[128];
          sprintf(topic, "master/%s/slaves/status", master_uid);
          MQTT_Queue_Publish(topic, payload);
          printf("MQTT: Queued updated online slaves list after config scan to %s -> %s\r\n", topic, payload);
          last_status_publish_tick = HAL_GetTick();
        }
        else
        {
          printf("MQTT: Disconnected. Cannot send updated online slaves list.\r\n");
        }
        
        // Trigger a write update for newly found slaves to sync screen state
        display_target_id = 0;
        display_updated = (online_slave_count > 0);
        
        // End config mode and return to normal operation immediately
        config_mode = false;
        printf("Modbus: Config Mode finished. Returning to normal operation.\r\n");
      }
    }
    else if (online_slave_count > 0)
    {
      if (display_updated)
      {
        display_updated = false;
        
        for (int sid = 1; sid <= MAX_SLAVE_DEVICES; sid++)
        {
          if (slave_registry[sid].pending_write)
          {
            slave_registry[sid].pending_write = 0; // Clear the pending flag
            
            // Check if this slave is online
            bool is_online = false;
            for (int i = 0; i < online_slave_count; i++)
            {
              if (online_slave_ids[i] == sid)
              {
                is_online = true;
                break;
              }
            }
            
            if (is_online)
            {
              write_telegram.u8id = sid;
              
              // Sync registry values to write buffer
              if (slave_registry[sid].is_ascii)
              {
                write_data[0] = ((uint16_t)slave_registry[sid].text[0] << 8) | (uint16_t)slave_registry[sid].text[1];
                write_data[1] = ((uint16_t)slave_registry[sid].text[2] << 8) | (uint16_t)slave_registry[sid].text[3];
                write_data[3] = (0x0100) | (uint16_t)slave_registry[sid].text[4];
              }
              else
              {
                int32_t val = (int32_t)strtol(slave_registry[sid].text, NULL, 10);
                write_data[0] = (uint16_t)((val >> 16) & 0xFFFF);
                write_data[1] = (uint16_t)(val & 0xFFFF);
                write_data[3] = (0x0000) | (slave_registry[sid].padding & 0xFF);
              }
              write_data[2] = slave_registry[sid].intensity;
              write_data[4] = slave_registry[sid].state;
              
              uint32_t result = 0;
              int retries = 3;
              while (retries > 0)
              {
                printf("Modbus: Writing to Slave %d (FC16)... val: %s (ascii: %d)\r\n", 
                       write_telegram.u8id, slave_registry[sid].text, slave_registry[sid].is_ascii);
                result = ModbusQueryV2(&hmaster, write_telegram);
                if (result == OP_OK_QUERY)
                {
                  printf("Modbus: Write to Slave %d success!\r\n", write_telegram.u8id);
                  break;
                }
                else
                {
                  retries--;
                  printf("Modbus: Write to Slave %d failed, err = %lu. Retrying (%d left)...\r\n", 
                         write_telegram.u8id, result, retries);
                  if (retries > 0)
                  {
                    osDelay(MODBUS_RETRY_DELAY_MS);
                  }
                }
              }
              
              // Publish status back to server
              extern bool mqtt_is_connected;
              if (mqtt_is_connected)
              {
                char ack_payload[128];
                if (result == OP_OK_QUERY)
                {
                  if (slave_registry[sid].is_ascii)
                  {
                    sprintf(ack_payload, "{\"status\":\"success\",\"id\":%d,\"val\":\"%s\",\"intensity\":%d,\"state\":%d,\"pad\":%d}", 
                            write_telegram.u8id, slave_registry[sid].text, slave_registry[sid].intensity, slave_registry[sid].state, slave_registry[sid].padding);
                  }
                  else
                  {
                    sprintf(ack_payload, "{\"status\":\"success\",\"id\":%d,\"val\":%s,\"intensity\":%d,\"state\":%d,\"pad\":%d}", 
                            write_telegram.u8id, slave_registry[sid].text, slave_registry[sid].intensity, slave_registry[sid].state, slave_registry[sid].padding);
                  }
                }
                else
                {
                  sprintf(ack_payload, "{\"status\":\"failed\",\"id\":%d,\"error\":\"timeout\"}", write_telegram.u8id);
                }
                extern char master_uid[25];
                char ack_topic[128];
                sprintf(ack_topic, "master/%s/display/status", master_uid);
                MQTT_Queue_Publish(ack_topic, ack_payload);
                printf("MQTT: Queued write status to %s -> %s\r\n", ack_topic, ack_payload);
              }
              osDelay(MODBUS_TRANSACTION_DELAY_MS);
            }
            else
            {
              // Targeted slave is not online, publish not_online failure ACK
              printf("Modbus: Targeted Slave %d is not online. Skipping write.\r\n", sid);
              extern bool mqtt_is_connected;
              if (mqtt_is_connected)
              {
                char ack_payload[128];
                sprintf(ack_payload, "{\"status\":\"failed\",\"id\":%d,\"error\":\"not_online\"}", sid);
                extern char master_uid[25];
                char ack_topic[128];
                sprintf(ack_topic, "master/%s/display/status", master_uid);
                MQTT_Queue_Publish(ack_topic, ack_payload);
                printf("MQTT: Queued write status to %s -> %s\r\n", ack_topic, ack_payload);
              }
            }
          }
        }
      }
      else
      {
        // Heartbeat read for all online slaves
        for (int i = 0; i < online_slave_count; i++)
        {
          if (display_updated) break; // Don't issue heartbeat if we have a pending display update command waiting
          uint8_t slave_id = online_slave_ids[i];
          read_telegram.u8id = slave_id;
          uint32_t result = ModbusQueryV2(&hmaster, read_telegram);
          if (result == OP_OK_QUERY)
          {
            uint8_t mode = (read_data[3] >> 8) & 0xFF;
            bool mismatch = false;
            
            // Check if intensity or display state mismatch
            if (read_data[2] != slave_registry[slave_id].intensity ||
                read_data[4] != slave_registry[slave_id].state)
            {
              mismatch = true;
            }
            
            if (mode == 0x01) { // Slave is in ASCII Mode
              if (slave_registry[slave_id].is_ascii != 1)
              {
                mismatch = true;
              }
              else
              {
                char h_str[6];
                h_str[0] = (read_data[0] >> 8) & 0xFF;
                h_str[1] = read_data[0] & 0xFF;
                h_str[2] = (read_data[1] >> 8) & 0xFF;
                h_str[3] = read_data[1] & 0xFF;
                h_str[4] = read_data[3] & 0xFF;
                h_str[5] = '\0';
                printf("Modbus: Heartbeat Slave %d ok. display text: %s (ASCII)\r\n", slave_id, h_str);
                
                if (strncmp(h_str, slave_registry[slave_id].text, 5) != 0)
                {
                  mismatch = true;
                }
              }
            } else { // Slave is in Numeric Mode
              if (slave_registry[slave_id].is_ascii != 0 ||
                  (read_data[3] & 0xFF) != slave_registry[slave_id].padding)
              {
                mismatch = true;
              }
              else
              {
                uint32_t current_val = ((uint32_t)read_data[0] << 16) | read_data[1];
                printf("Modbus: Heartbeat Slave %d ok. display val: %lu\r\n", slave_id, current_val);
                
                uint32_t expected_val = (uint32_t)strtol(slave_registry[slave_id].text, NULL, 10);
                if (current_val != expected_val)
                {
                  mismatch = true;
                }
              }
            }
            
            if (mismatch)
            {
              printf("Modbus: Slave %d display mismatch detected! Force syncing...\r\n", slave_id);
              slave_registry[slave_id].pending_write = 1;
              display_updated = true;
            }
            
            online_slave_fail_count[slave_id] = 0; // Reset fail count
          }
          else
          {
            printf("Modbus: Heartbeat Slave %d failed, err = %lu\r\n", slave_id, result);
            online_slave_fail_count[slave_id]++;
            if (online_slave_fail_count[slave_id] >= HEARTBEAT_CONSECUTIVE_FAILS)
            {
              printf("Modbus: Slave %d failed %d consecutive heartbeats. Removing from online list.\r\n", slave_id, HEARTBEAT_CONSECUTIVE_FAILS);
              online_slave_fail_count[slave_id] = 0;
              
              // Shift elements to remove the offline slave from the active registry
              for (int j = i; j < online_slave_count - 1; j++)
              {
                online_slave_ids[j] = online_slave_ids[j + 1];
              }
              online_slave_count--;
              i--; // Adjust index since list size decreased
              
              // Force immediate status publish to MQTT
              last_status_publish_tick = 0;
            }
          }
          osDelay(MODBUS_TRANSACTION_DELAY_MS); // Brief delay between transactions
        }
      }
      
      // Periodic status publish to MQTT (every 10 seconds)
      uint32_t now = HAL_GetTick();
      if (now - last_status_publish_tick >= STATUS_PUBLISH_INTERVAL_MS || last_status_publish_tick == 0)
      {
        last_status_publish_tick = now;
        extern bool mqtt_is_connected;
        if (mqtt_is_connected)
        {
          static char status_payload[1024];
          int pos = sprintf(status_payload, "{\"online_slaves\":[");
          for (int i = 0; i < online_slave_count; i++)
          {
            pos += sprintf(status_payload + pos, "%d", online_slave_ids[i]);
            if (i < online_slave_count - 1)
            {
              pos += sprintf(status_payload + pos, ",");
            }
          }
          sprintf(status_payload + pos, "]}");
          
          extern char master_uid[25];
          char status_topic[128];
          sprintf(status_topic, "master/%s/slaves/status", master_uid);
          MQTT_Queue_Publish(status_topic, status_payload);
          printf("MQTT: Queued periodic online slaves list -> %s\r\n", status_payload);
        }
      }
      
      if (display_update_sem != NULL)
      {
        osSemaphoreAcquire(display_update_sem, pdMS_TO_TICKS(2000));
      }
      else
      {
        osDelay(2000);
      }
    }
    else
    {
      // No active slaves registered, publish empty status list periodically
      uint32_t now = HAL_GetTick();
      if (now - last_status_publish_tick >= STATUS_PUBLISH_INTERVAL_MS || last_status_publish_tick == 0)
      {
        last_status_publish_tick = now;
        extern bool mqtt_is_connected;
        if (mqtt_is_connected)
        {
          extern char master_uid[25];
          char status_topic[128];
          sprintf(status_topic, "master/%s/slaves/status", master_uid);
          MQTT_Queue_Publish(status_topic, "{\"online_slaves\":[]}");
          printf("MQTT: Queued periodic empty online slaves list\r\n");
        }
      }
      if (display_update_sem != NULL)
      {
        osSemaphoreAcquire(display_update_sem, pdMS_TO_TICKS(500));
      }
      else
      {
        osDelay(500);
      }
    }
  }
  /* USER CODE END StartModbusAppTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
