/* USER CODE BEGIN Header */
/**
  *=============================================================================
  * @file    : main.c
  * @brief   : Main program body
  *=============================================================================
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  *=============================================================================
  */
/**=============================================================================
  * trail-hud hardware wiring
  * STM32H750B-DK + HM-10 AT-09 BLE + Keyestudio MPU-6050 + protoboard
  *
  *=============================================================================
  * PROTOBOARD / SHARED POWER BUSES
  *=============================================================================
  *
  * STM32H750B-DK CN3 pin 6 GND -> protoboard bus GND
  *
  *
  *=============================================================================
  * HM-10 BLE MODULE
  *=============================================================================
  *
  * STM32H750B-DK CN3 pin 5 5V -> HM-10 HM-10 AT-09 VCC
  * protoboard bus GND -> HM-10 HM-10 AT-09 GND
  *
  * HM-10 HM-10 AT-09 TXD / UART_TX -> STM32H750B-DK CN2 D11 / PB15 / USART1_RX
  * HM-10 HM-10 AT-09 RXD / UART_RX -> STM32H750B-DK STMod+ P1 pin 9 / PB14 / USART1_TX
  * HM-10 HM-10 AT-09 STATE -> STM32H750B-DK CN6 D2 / PG3
  * HM-10 HM-10 AT-09 EN -> STM32H750B-DK CN6 D4 / PK1
  *
  *
  *=============================================================================
  * KEYESTUDIO MPU-6050 GYROSCOPE / ACCELEROMETER MODULE
  *=============================================================================
  *
  * STM32H750B-DK CN3 pin 4 3V3 -> Keyestudio MPU-6050 VCC
  * protoboard bus GND -> Keyestudio MPU-6050 GND
  *
  * Keyestudio MPU-6050 SCL -> STM32H750B-DK CN2 D15 / PD12 / I2C4_SCL
  * Keyestudio MPU-6050 SDA -> STM32H750B-DK CN2 D14 / PD13 / I2C4_SDA
  * Keyestudio MPU-6050 AD0 -> protoboard bus GND
  * Keyestudio MPU-6050 INT -> STM32H750B-DK CN2 D9 / PH15
  *
  * Keyestudio MPU-6050 XDA -> no connection
  * Keyestudio MPU-6050 XCL -> no connection
  *
  *=============================================================================
  */
/**=============================================================================
  * SERIAL DEBUG TERMINAL / ST-LINK VIRTUAL COM PORT
  *=============================================================================
  *
  * Purpose:
  * - Use USART3 only for local PC-side firmware debugging.
  * - Keep USART3 separate from the HM-10 / BT05 / AT-09 BLE data path.
  *
  * Communication path:
  *   STM32 firmware -> USART3 -> ST-LINK Virtual COM Port -> PuTTY
  *
  * PuTTY settings:
  * - Port        : STMicroelectronics STLink Virtual COM Port
  * - Baud        : 9600
  * - Data bits   : 8
  * - Stop bits   : 1
  * - Parity      : None
  * - Flow control: None
  *
  * Notes:
  * - On Windows, use the COM port shown in Device Manager under
  *   "Ports (COM & LPT)".
  * - Expected debug output includes firmware status messages, HM-10 STATE
  *   changes, and USART1 receive logs.
  *
  *
  *=============================================================================
  * PHONE BLE TERMINAL / HM-10 UART BRIDGE
  *=============================================================================
  *
  * Purpose:
  * - Use the phone terminal to validate the BLE data path through the HM-10.
  * - Confirm both directions:
  *   - STM32 -> phone
  *   - phone -> STM32
  *
  * Communication path:
  *   Phone app <-> BLE <-> HM-10 <-> USART1 <-> STM32 firmware
  *
  * Recommended Android app:
  * - Serial Bluetooth Terminal by Kai Morich
  *
  * Serial Bluetooth Terminal app setup:
  * - Open Devices.
  * - Select the Bluetooth LE / BLE list, not Bluetooth Classic.
  * - Scan for BT05 or the configured HM-10 name.
  * - Connect and stay on the main terminal screen.
  *
  * Expected behavior:
  * - When connected, PuTTY should show HM-10 STATE as CONNECTED.
  * - STM32 -> phone test: the phone should receive heartbeat messages.
  * - Phone -> STM32 test: send "x" from the phone.
  * - PuTTY should print, up to two sequential strings per request:
  *
  *     USART1 RX byte: 0x78 'x'
  *
  * - If echo is enabled, the phone should receive "x" back.
  *
  *=============================================================================
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hm10.h"
#include "debug_terminal.h"
#include "mpu6050.h"
#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BLE_RX_LINE_SIZE               220U
#define MPU6050_DEBUG_UPDATE_PERIOD_MS 1000U
#define TRAIL_HUD_LCD_INSTANCE         0U

#define TRAIL_HUD_COLOR_LIGHT_OLIVE    0xFF6B7540UL
#define TRAIL_TITLE                    "TRAIL-MODULE"
#define TRAIL_TITLE_X                  8U
#define TRAIL_TITLE_Y                  8U
#define TRAIL_TITLE_CHAR_COUNT         ((uint32_t)(sizeof(TRAIL_TITLE) - 1U))
#define TRAIL_TITLE_BOX_X              4U
#define TRAIL_TITLE_BOX_Y              4U
#define TRAIL_TITLE_BOX_WIDTH          215U
#define TRAIL_TITLE_BOX_HEIGHT         34U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c4;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* USER CODE BEGIN PV */
static HM10_HandleTypeDef hm10;
static MPU6050_HandleTypeDef mpu6050;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C4_Init(void);
void StartDefaultTask(void* argument);

/* USER CODE BEGIN PFP */
static uint8_t DebugTask_ReadBleRx(DebugTerminalMode mode,
                                   char* ble_rx_line,
                                   uint16_t* ble_rx_len,
                                   uint16_t ble_rx_line_size);
static uint8_t DebugTask_WaitForPingReply(uint32_t timeout_ms,
                                          char* ble_rx_line,
                                          uint16_t* ble_rx_len,
                                          uint16_t ble_rx_line_size);
static void DebugTask_RunPingSequence(DebugTerminalMode* debug_mode,
                                      char* ble_rx_line,
                                      uint16_t* ble_rx_len,
                                      uint16_t ble_rx_line_size);
static void DebugTask_PrintMpu6050Data(uint32_t* last_tick);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t DebugTask_ReadBleRx(DebugTerminalMode mode,
                                   char* ble_rx_line,
                                   uint16_t* ble_rx_len,
                                   uint16_t ble_rx_line_size)
{
    uint8_t rx_byte = 0U;
    uint8_t ping_reply_received = 0U;

    while (HM10_ReadByte(&hm10, &rx_byte, 1U) == HM10_OK)
    {
        if (DebugTerminal_HandleBleRxByte(&huart3,
                                          rx_byte,
                                          ble_rx_line,
                                          ble_rx_len,
                                          ble_rx_line_size,
                                          mode) != 0U)
        {
            ping_reply_received = 1U;
        }
    }

    return ping_reply_received;
}

static uint8_t DebugTask_WaitForPingReply(uint32_t timeout_ms,
                                          char* ble_rx_line,
                                          uint16_t* ble_rx_len,
                                          uint16_t ble_rx_line_size)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if (DebugTask_ReadBleRx(DEBUG_TERMINAL_MODE_PINGS,
                                ble_rx_line,
                                ble_rx_len,
                                ble_rx_line_size) != 0U)
        {
            return 1U;
        }

        osDelay(1U);
    }

    return 0U;
}

static void DebugTask_RunPingSequence(DebugTerminalMode* debug_mode,
                                      char* ble_rx_line,
                                      uint16_t* ble_rx_len,
                                      uint16_t ble_rx_line_size)
{
    uint8_t ping_index;

    if ((debug_mode == NULL) || (ble_rx_line == NULL) || (ble_rx_len == NULL))
    {
        return;
    }

    *ble_rx_len = 0U;
    ble_rx_line[0] = '\0';

    DebugTerminal_PrintLine(&huart3, "BLE: pinging phone with 4 packets of data");

    for (ping_index = 0U; ping_index < DEBUG_TERMINAL_PING_COUNT; ping_index++)
    {
        uint8_t reply_received = 0U;

        if (HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin) == GPIO_PIN_SET)
        {
            if (HM10_SendString(&hm10, DEBUG_TERMINAL_PING_PACKET "\r\n") == HM10_OK)
            {
                reply_received = DebugTask_WaitForPingReply(DEBUG_TERMINAL_PING_TIMEOUT_MS,
                                                            ble_rx_line,
                                                            ble_rx_len,
                                                            ble_rx_line_size);
            }
        }

        DebugTerminal_PrintLine(&huart3,
                                (reply_received != 0U)
                                    ? "BLE: reply from phone"
                                    : "BLE: no reply");
    }

    *debug_mode = DEBUG_TERMINAL_MODE_WAITING;
    DebugTerminal_PrintMode(&huart3, *debug_mode);
}

static void DebugTask_PrintMpu6050Data(uint32_t* last_tick)
{
    MPU6050_DataPacket packet;
    MPU6050_StatusTypeDef status;

    if (last_tick == NULL)
    {
        return;
    }

    if ((HAL_GetTick() - *last_tick) < MPU6050_DEBUG_UPDATE_PERIOD_MS)
    {
        return;
    }

    status = MPU6050_ReadDataPacket(&mpu6050, &packet);

    if (status == MPU6050_OK)
    {
        DebugTerminal_PrintMpu6050Packet(&huart3, &packet);
    }
    else
    {
        DebugTerminal_PrintLine(&huart3, "MPU-6050: read failed");
    }

    *last_tick = HAL_GetTick();
}

static void DrawTitleText(void)
{
    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_DisplayStringAt(
        TRAIL_TITLE_X,
        TRAIL_TITLE_Y,
        (uint8_t *)TRAIL_TITLE,
        LEFT_MODE
    );

    UTIL_LCD_DrawHLine(
        TRAIL_TITLE_X,
        TRAIL_TITLE_Y + Font24.Height + 1U,
        TRAIL_TITLE_CHAR_COUNT * Font24.Width,
        UTIL_LCD_COLOR_BLACK
    );
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    /* MPU Configuration--------------------------------------------------------*/
    MPU_Config();

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
    MX_USART3_UART_Init();
    MX_USART1_UART_Init();
    MX_I2C4_Init();
    /* USER CODE BEGIN 2 */
    DebugTerminal_PrintTitle(&huart3);

    if (HM10_Init(&hm10, &huart1) != HM10_OK)
    {
        Error_Handler();
    }

    HM10_SetNameAndReset(&hm10, "Trail-Module", 1000U);
    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized HM-10 on USART1");

    if (MPU6050_Init(&mpu6050, &hi2c4, MPU6050_DEFAULT_I2C_ADDRESS) != MPU6050_OK)
    {
        Error_Handler();
    }
    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized MPU-6050 on I2C4");

    if (BSP_LCD_Init(TRAIL_HUD_LCD_INSTANCE, LCD_ORIENTATION_LANDSCAPE) != BSP_ERROR_NONE)
    {
        Error_Handler();
    }

    BSP_LCD_DisplayOn(TRAIL_HUD_LCD_INSTANCE);
    BSP_LCD_SetBrightness(TRAIL_HUD_LCD_INSTANCE, 100U);
    BSP_LCD_SetActiveLayer(TRAIL_HUD_LCD_INSTANCE, 0U);

    UTIL_LCD_SetFuncDriver(&LCD_Driver);
    UTIL_LCD_SetLayer(0U);

    UTIL_LCD_Clear(UTIL_LCD_COLOR_BLACK);

    UTIL_LCD_FillRect(0U, 0U, 96U, 136U, 0xFF55642AUL);
    UTIL_LCD_FillRect(96U, 0U, 96U, 136U, 0xFF607036UL);
    UTIL_LCD_FillRect(192U, 0U, 96U, 136U, 0xFF6B7540UL);
    UTIL_LCD_FillRect(288U, 0U, 96U, 136U, 0xFF78824BUL);
    UTIL_LCD_FillRect(384U, 0U, 96U, 136U, 0xFF869058UL);

    UTIL_LCD_FillRect(0U, 136U, 480U, 2U, UTIL_LCD_COLOR_BLACK);

    UTIL_LCD_FillRect(0U, 138U, 96U, 134U, 0xFF626D3BUL);
    UTIL_LCD_FillRect(96U, 138U, 96U, 134U, 0xFF66713EUL);
    UTIL_LCD_FillRect(192U, 138U, 96U, 134U, 0xFF6B7540UL);
    UTIL_LCD_FillRect(288U, 138U, 96U, 134U, 0xFF707943UL);
    UTIL_LCD_FillRect(384U, 138U, 96U, 134U, 0xFF757D46UL);

    UTIL_LCD_FillRect(TRAIL_TITLE_BOX_X,
                  TRAIL_TITLE_BOX_Y,
                  TRAIL_TITLE_BOX_WIDTH,
                  TRAIL_TITLE_BOX_HEIGHT,
                  UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_DrawRect(TRAIL_TITLE_BOX_X,
                      TRAIL_TITLE_BOX_Y,
                      TRAIL_TITLE_BOX_WIDTH,
                      TRAIL_TITLE_BOX_HEIGHT,
                      UTIL_LCD_COLOR_BLACK);

    DrawTitleText();
    /* USER CODE END 2 */

    /* Init scheduler */
    osKernelInitialize();

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
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

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.CSIState = RCC_CSI_OFF;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
        | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief I2C4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C4_Init(void)
{
    /* USER CODE BEGIN I2C4_Init 0 */

    /* USER CODE END I2C4_Init 0 */

    /* USER CODE BEGIN I2C4_Init 1 */

    /* USER CODE END I2C4_Init 1 */
    hi2c4.Instance = I2C4;
    hi2c4.Init.Timing = 0x00707CBB;
    hi2c4.Init.OwnAddress1 = 0;
    hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c4.Init.OwnAddress2 = 0;
    hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c4) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Analogue filter
    */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Digital filter
    */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C4_Init 2 */

    /* USER CODE END I2C4_Init 2 */
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
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART1_Init 2 */
    /* USER CODE END USART1_Init 2 */
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{
    /* USER CODE BEGIN USART3_Init 0 */
    /* USER CODE END USART3_Init 0 */

    /* USER CODE BEGIN USART3_Init 1 */
    /* USER CODE END USART3_Init 1 */
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 9600;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART3_Init 2 */
    /* USER CODE END USART3_Init 2 */
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
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(HM10_EN_GPIO_Port, HM10_EN_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin : HM10_EN_Pin */
    GPIO_InitStruct.Pin = HM10_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HM10_EN_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : HM10_STATE_Pin */
    GPIO_InitStruct.Pin = HM10_STATE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HM10_STATE_GPIO_Port, &GPIO_InitStruct);

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
void StartDefaultTask(void* argument)
{
    /* USER CODE BEGIN 5 */
    char ble_rx_line[BLE_RX_LINE_SIZE] = {0};
    uint16_t ble_rx_len = 0U;
    GPIO_PinState last_state;
    DebugTerminalMode debug_mode = DEBUG_TERMINAL_MODE_WAITING;
    uint32_t last_mpu6050_tick = HAL_GetTick() - MPU6050_DEBUG_UPDATE_PERIOD_MS;

    (void)argument;
    DebugTerminal_PrintMode(&huart3, debug_mode);

    last_state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);
    DebugTerminal_PrintLine(&huart3,
                            (last_state == GPIO_PIN_SET)
                                ? "BLE: connection established"
                                : "BLE: connection terminated");

    for (;;)
    {
        GPIO_PinState state;

        state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);

        DebugTerminal_HandleInput(&huart3, &debug_mode);

        if (debug_mode == DEBUG_TERMINAL_MODE_PINGS)
        {
            DebugTask_RunPingSequence(&debug_mode,
                                      ble_rx_line,
                                      &ble_rx_len,
                                      sizeof(ble_rx_line));
            osDelay(1U);
            continue;
        }

        if (state != last_state)
        {
            last_state = state;
            DebugTerminal_PrintLine(&huart3,
                                    (state == GPIO_PIN_SET)
                                        ? "BLE: connection established"
                                        : "BLE: connection terminated");
        }

        if (debug_mode == DEBUG_TERMINAL_MODE_MPU6050_DATA)
        {
            DebugTask_PrintMpu6050Data(&last_mpu6050_tick);
        }

        (void)DebugTask_ReadBleRx(debug_mode,
                                  ble_rx_line,
                                  &ble_rx_len,
                                  sizeof(ble_rx_line));

        osDelay(1U);
    }
    /* USER CODE END 5 */
}

/* MPU Configuration */

void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    /*
     * Region 0: default protection region.
     * This is your existing generated region.
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x00000000U;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 1: external SDRAM / LCD framebuffer.
     *
     * LCD_LAYER_0_ADDRESS in the working BSP project is 0xD0000000.
     * Use a non-cacheable region so CPU pixel writes are visible to LTDC.
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0xD0000000U;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4MB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
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
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
