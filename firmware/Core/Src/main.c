/* USER CODE BEGIN Header */
/**
  *=============================================================================
  * @file           : main.c
  * @brief          : Main program body
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
  * protoboard bus GND         -> HM-10 HM-10 AT-09 GND
  *
  * HM-10 HM-10 AT-09 TXD / UART_TX -> STM32H750B-DK CN2 D11         / PB15 / USART1_RX
  * HM-10 HM-10 AT-09 RXD / UART_RX -> STM32H750B-DK STMod+ P1 pin 9 / PB14 / USART1_TX
  * HM-10 HM-10 AT-09 STATE         -> STM32H750B-DK CN6 D2 / PG3
  * HM-10 HM-10 AT-09 EN            -> STM32H750B-DK CN6 D4 / PK1
  *
  *
  *=============================================================================
  * KEYESTUDIO MPU-6050 GYROSCOPE / ACCELEROMETER MODULE
  *=============================================================================
  *
  * STM32H750B-DK CN3 pin 4 3V3 -> Keyestudio MPU-6050 VCC
  * protoboard bus GND          -> Keyestudio MPU-6050 GND
  *
  * Keyestudio MPU-6050 SCL -> STM32H750B-DK CN2 D15 / PD12 / I2C4_SCL
  * Keyestudio MPU-6050 SDA -> STM32H750B-DK CN2 D14 / PD13 / I2C4_SDA
  * Keyestudio MPU-6050 AD0 -> protoboard bus GND
  * Keyestudio MPU-6050 INT -> STM32H750B-DK CN2 D9 / PH15
  *
  * Keyestudio MPU-6050 XDA -> no connection
  * Keyestudio MPU-6050 XCL -> no connection
  *
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
#include <stdio.h>
#include <string.h>
#include "hm10.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBUG_PRINT_SIZE 220U
#define BLE_RX_LINE_SIZE 220U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

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
static char debug_print[DEBUG_PRINT_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
void StartDebugTask(void* argument);

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
    /* USER CODE BEGIN 2 */

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
    defaultTaskHandle = osThreadNew(StartDebugTask, NULL, &defaultTask_attributes);

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

    /** Supply configuration update enable
    */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
        | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
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
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(HM10_EN_GPIO_Port, HM10_EN_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin : HM10_STATE_Pin */
    GPIO_InitStruct.Pin = HM10_STATE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HM10_STATE_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : HM10_EN_Pin */
    GPIO_InitStruct.Pin = HM10_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HM10_EN_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void PrintDebugTitle(void)
{
    static const char boot[] =
        "\r\n"
        "+===========================================================+\r\n"
        "| TRAIL-HUD STM32 DEBUG TERMINAL                            |\r\n"
        "+===========================================================+\r\n"
        "| Board : STM32H750B-DK                                     |\r\n"
        "| BLE   : HM-10 / AT-09 on USART1                           |\r\n"
        "| Debug : USART3 / ST-LINK VCP / PuTTY / 9600 8N1           |\r\n"
        "+-----------------------------------------------------------+\r\n"
        "| Status: firmware started, waiting for BLE activity        |\r\n"
        "+===========================================================+\r\n"
        "\r\n";

    HAL_UART_Transmit(&huart3,
                      (uint8_t*)boot,
                      sizeof(boot) - 1U,
                      2000U);
}

static void PrintDebugLine(const char* text)
{
    int len;

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> %s\r\n",
                   (text != NULL) ? text : "(null)");

    if (len <= 0) return;
    if (len >= sizeof(debug_print)) len = sizeof(debug_print) - 1;

    HAL_UART_Transmit(&huart3,
                      (uint8_t*)debug_print,
                      (uint16_t)len,
                      1000U);
}

static void PrintBlePacketDebug(const char* packet)
{
    int len;

    if (packet == NULL) return;

    if ((packet[0] == '[') && (packet[strlen(packet) - 1U] == ']')) { PrintDebugLine("BLE <- PHONE: phone pose packet received"); }
    else { PrintDebugLine("BLE <- PHONE: unrecognized line received"); }

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "  %s\r\n",
                   packet);

    if (len <= 0) return;
    if (len >= (int)sizeof(debug_print)) len = (int)sizeof(debug_print) - 1;

    HAL_UART_Transmit(&huart3,
                      (uint8_t*)debug_print,
                      (uint16_t)len,
                      1000U);
}

static void HandleBleRxByte(uint8_t rx_byte, char* rx_line, uint16_t* rx_len, uint16_t rx_line_size)
{
    if ((rx_line == NULL) || (rx_len == NULL) || (rx_line_size == 0U)) return;
    if (rx_byte == '\r') return;

    if (rx_byte == '\n')
    {
        rx_line[*rx_len] = '\0';

        if (*rx_len > 0U) PrintBlePacketDebug(rx_line);

        *rx_len = 0U;
        rx_line[0] = '\0';
        return;
    }

    if (*rx_len < (uint16_t)(rx_line_size - 1U))
    {
        rx_line[*rx_len] = (char)rx_byte;
        (*rx_len)++;
    }
    else
    {
        *rx_len = 0U;
        rx_line[0] = '\0';
        PrintDebugLine("BLE <- PHONE: RX line overflow, dropped partial packet");
    }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDebugTask(void* argument)
{
    /* USER CODE BEGIN 5 */

    uint32_t last_tick = 0U;
    uint32_t counter = 0U;
    uint8_t rx_byte = 0U;
    char ble_rx_line[BLE_RX_LINE_SIZE] = {0};
    uint16_t ble_rx_len = 0U;
    GPIO_PinState last_state;

    (void)argument;

    PrintDebugTitle();

    last_state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);
    PrintDebugLine((last_state == GPIO_PIN_SET)
                       ? "HM10 STATE: CONNECTED"
                       : "HM10 STATE: DISCONNECTED");

    for (;;)
    {
        GPIO_PinState state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);

        if (state != last_state)
        {
            last_state = state;

            PrintDebugLine((state == GPIO_PIN_SET)
                               ? "HM10 STATE: CONNECTED"
                               : "HM10 STATE: DISCONNECTED");
        }

        if ((state == GPIO_PIN_SET) && ((HAL_GetTick() - last_tick) >= 1000U))
        {
            char ble_msg[48];
            int ble_len = snprintf(ble_msg,
                                   sizeof(ble_msg),
                                   "stm32 heartbeat %lu\r\n",
                                   (unsigned long)counter++);

            if ((ble_len > 0) && (ble_len < (int)sizeof(ble_msg)))
            {
                (void)HAL_UART_Transmit(&huart1,
                                        (uint8_t*)ble_msg,
                                        (uint16_t)ble_len,
                                        500U);

                PrintDebugLine("USART1 -> HM10: heartbeat sent");
            }

            last_tick = HAL_GetTick();
        }

        while (HAL_UART_Receive(&huart1, &rx_byte, 1U, 1U) == HAL_OK)
        {
            HandleBleRxByte(rx_byte,
                            ble_rx_line,
                            &ble_rx_len,
                            sizeof(ble_rx_line));
        }

        osDelay(1);
    }

    /* USER CODE END 5 */
}

/* MPU Configuration */

void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    /* Disables the MPU */
    HAL_MPU_Disable();

    /** Initializes and configures the Region and the memory to be protected
    */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    /* Enables the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
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
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
