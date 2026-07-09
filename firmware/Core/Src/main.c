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
  * - Scan for BT05 or the configured HM-10 name (set to TRAIL-HUD by default).
  * - Connect and stay on the main terminal screen.
  *
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
#include "mpu6050.h"
#include "debug_terminal.h"
#include "trail_gui.h"
#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"

#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    GPIO_TypeDef* GPIO_Port;
    uint16_t GPIO_Pin;
    uint16_t GPIO_DefaultState;
} LED_HandleTypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BLE_RX_LINE_SIZE 220U
#define MPU6050_DEBUG_UPDATE_PERIOD_MS 1000U
#define TRAIL_HUD_LCD_INSTANCE 0U
#define TRAIL_HUD_LOADING_STAGE_COUNT 4U
#define PHONE_RENDER_LINE_WIDTH 1U
#define PHONE_RENDER_LINE_COLOR UTIL_LCD_COLOR_WHITE
#define PHONE_RENDER_CLEAR_COLOR UTIL_LCD_COLOR_BLACK
#define LED_HANDLE_COUNT 3U

#define TRAIL_GUI_PHONE_RENDER_MARGIN 6U
#define TRAIL_GUI_PHONE_RENDER_PADDING 6U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static HM10_HandleTypeDef hm10;
static MPU6050_HandleTypeDef mpu6050;

osSemaphoreId_t hm10TopLED;
osSemaphoreId_t hm10BottomLED;
osMessageQueueId_t hm10RenderInstructionQueue;

static uint8_t hm10_uart_rx_byte;
static uint16_t hm10_isr_line_len = 0U;
static char hm10_isr_line[BLE_RX_LINE_SIZE];
static volatile uint8_t hm10_ping_reply_flag = 0U;
static volatile DebugTerminalMode debug_terminal_mode = DEBUG_TERMINAL_MODE_WAITING;
static volatile GPIO_PinState hm10_connection_state = GPIO_PIN_RESET;

static const LED_HandleTypeDef led_handles[LED_HANDLE_COUNT] = {
    {GPIOD, GPIO_PIN_3, GPIO_PIN_RESET},
    {GPIOJ, GPIO_PIN_2, GPIO_PIN_SET},
    {GPIOI, GPIO_PIN_13, GPIO_PIN_SET},
};
static const TrailGui_BoundingBox phone_render_bounds = {
    .x_min = 241U + (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
    .x_max = 479U - (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
    .y_min = 0U + (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
    .y_max = 239U - (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
};
static const TrailGui_BoundingBox phone_render_padding_bounds = {
    .x_min = phone_render_bounds.x_min - TRAIL_GUI_PHONE_RENDER_PADDING,
    .x_max = phone_render_bounds.x_max + TRAIL_GUI_PHONE_RENDER_PADDING,
    .y_min = phone_render_bounds.y_min - TRAIL_GUI_PHONE_RENDER_PADDING,
    .y_max = phone_render_bounds.y_max + TRAIL_GUI_PHONE_RENDER_PADDING,
};
static const TrailGui_BoundingBox phone_gps_bounds = {
    .x_min = phone_render_bounds.x_min,
    .x_max = phone_render_bounds.x_max,
    .y_min = phone_render_padding_bounds.y_max + (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
    .y_max = 272U - (TRAIL_GUI_PHONE_RENDER_MARGIN + TRAIL_GUI_PHONE_RENDER_PADDING),
};
static const TrailGui_BoundingBox phone_gps_padding_bounds = {
    .x_min = phone_gps_bounds.x_min - TRAIL_GUI_PHONE_RENDER_PADDING,
    .x_max = phone_gps_bounds.x_max + TRAIL_GUI_PHONE_RENDER_PADDING,
    .y_min = phone_gps_bounds.y_min - TRAIL_GUI_PHONE_RENDER_PADDING,
    .y_max = phone_gps_bounds.y_max + TRAIL_GUI_PHONE_RENDER_PADDING,
};

const osThreadAttr_t MainThread_attributes = {
    .name = "MainThread",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

const osThreadAttr_t HM10Thread_attributes = {
    .name = "HM10Thread",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C4_Init(void);
void MainThread(void* argument);

/* USER CODE BEGIN PFP */
static void DebugTask_ClearPhoneRenderArea(void);
static void DebugTask_RenderPhoneFrame(const HM10_DataPacket* hm10_packet);
static uint8_t DebugTask_WaitForPingReply(uint32_t timeout_ms);
static void DebugTask_RunPingSequence(DebugTerminalMode* debug_mode);
static void DebugTask_PrintMpu6050Data(uint32_t* last_tick);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Toggles a LED sequence, in the same order as is defined in the led_handles array.
 * @param delay_ms Total time in milliseconds the whole sequence takes; split
 *                 evenly between each LED transition.
 * @return None.
 */
static void LED_ToggleSequence(uint32_t delay_ms)
{
    if (LED_HANDLE_COUNT < 1U)
    {
        return;
    }

    uint32_t split_delay = delay_ms / 2;
    HAL_GPIO_TogglePin(led_handles[0].GPIO_Port, led_handles[0].GPIO_Pin);
    for (uint16_t led = 1U; led < LED_HANDLE_COUNT; led++)
    {
        HAL_Delay(split_delay);
        HAL_GPIO_TogglePin(led_handles[led].GPIO_Port, led_handles[led].GPIO_Pin);
        HAL_Delay(split_delay);
        HAL_GPIO_TogglePin(led_handles[led - 1].GPIO_Port, led_handles[led - 1].GPIO_Pin);
    }
    HAL_Delay(split_delay);
    HAL_GPIO_TogglePin(led_handles[LED_HANDLE_COUNT - 1].GPIO_Port, led_handles[LED_HANDLE_COUNT - 1].GPIO_Pin);
}

/**
 * @brief Fills the phone render area with the solid clear color.
 * @param None.
 * @return None.
 */
static void DebugTask_ClearPhoneRenderArea(void)
{
    TrailGui_DrawRoundedRectangle(phone_render_bounds, 0U, PHONE_RENDER_CLEAR_COLOR);
    TrailGui_DrawRoundedRectangle(phone_gps_bounds, 0U, PHONE_RENDER_CLEAR_COLOR);
}

/**
 * @brief Redraws the phone orientation cuboid for one parsed HM-10 packet.
 * @param hm10_packet Parsed HM-10 data packet holding the phone orientation
 *                    quaternion fields; NULL is not allowed. The render is
 *                    skipped when hm10_connection_state indicates the BLE
 *                    link is not currently established.
 * @return None.
 */
static void DebugTask_RenderPhoneFrame(const HM10_DataPacket* hm10_packet)
{
    if (hm10_packet == NULL)
    {
        return;
    }

    if (hm10_connection_state != GPIO_PIN_SET)
    {
        return;
    }

    DebugTask_ClearPhoneRenderArea();
    TrailGui_RenderPhoneCuboid(hm10_packet,
                             phone_render_bounds,
                             PHONE_RENDER_LINE_WIDTH,
                             PHONE_RENDER_LINE_COLOR);
    TrailGui_RenderPhoneGps(hm10_packet, phone_gps_bounds, UTIL_LCD_COLOR_WHITE);
}

/**
 * @brief Blocks until a ping reply is received over BLE or the timeout elapses.
 * @param timeout_ms Maximum time to wait for a matching reply in milliseconds.
 *                   The function yields to the RTOS scheduler between polls.
 * @return 1 when a ping reply was received before the timeout expired; 0 when
 *         the timeout elapsed without a matching reply.
 */
static uint8_t DebugTask_WaitForPingReply(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    hm10_ping_reply_flag = 0U;

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if (hm10_ping_reply_flag != 0U)
        {
            hm10_ping_reply_flag = 0U;
            return 1U;
        }

        osDelay(1U);
    }

    return 0U;
}

/**
 * @brief Sends a fixed-count BLE ping sequence and reports each reply result.
 * @param debug_mode Pointer to the active debug terminal mode. NULL is not
 *                   allowed. Set to DEBUG_TERMINAL_MODE_WAITING after the
 *                   full sequence completes.
 * @return None.
 */
static void DebugTask_RunPingSequence(DebugTerminalMode* debug_mode)
{
    DebugTerminal_PrintLine(&huart3, "BLE: pinging phone with 4 packets of data");

    for (uint8_t ping_index = 0U; ping_index < DEBUG_TERMINAL_PING_COUNT; ping_index++)
    {
        uint8_t reply_received = 0U;

        if (hm10_connection_state == GPIO_PIN_SET)
        {
            if (HM10_SendString(&hm10, DEBUG_TERMINAL_PING_PACKET "\r\n") == HM10_OK)
            {
                reply_received = DebugTask_WaitForPingReply(DEBUG_TERMINAL_PING_TIMEOUT_MS);
            }
        }

        DebugTerminal_PrintLine(&huart3, (reply_received != 0U) ? "BLE: reply from phone" : "BLE: no reply");
        osDelay(500U);
    }

    *debug_mode = DEBUG_TERMINAL_MODE_WAITING;
    DebugTerminal_PrintMode(&huart3, *debug_mode);
}

/**
 * @brief Reads and prints MPU-6050 sensor data at a fixed periodic interval.
 * @param last_tick Pointer to the HAL tick value recorded at the last print.
 *                  NULL is not allowed. Updated to the current tick after each
 *                  read attempt, whether the read succeeds or fails.
 * @return None.
 */
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

/**
 * @brief Toggles the top green LED (PD3) for 100ms when a semaphore token is available.
 * @param argument Does not impact the result.
 * @return None.
 */
void HM10_TopLEDThread(void* argument)
{
    while (1)
    {
        osSemaphoreAcquire(hm10TopLED, osWaitForever);
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3);
        osDelay(100);
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3);
    }
}

/**
 * @brief Toggles the bottom green LED (PJ2) for 100ms when a semaphore token is available.
 * @param argument Does not impact the result.
 * @return None.
 */
void HM10_BottomLEDThread(void* argument)
{
    while (1)
    {
        osSemaphoreAcquire(hm10BottomLED, osWaitForever);
        HAL_GPIO_TogglePin(GPIOJ, GPIO_PIN_2);
        osDelay(100);
        HAL_GPIO_TogglePin(GPIOJ, GPIO_PIN_2);
    }
}

/**
 * @brief Consumes hm10RenderInstructionQueue and applies each message to the LCD/debug terminal.
 * @param argument Does not impact the result.
 * @return None.
 */
void HM10_Thread(void* argument)
{
    TrailGui_RenderWidgetPacket packet;

    while (1)
    {
        // Wait until the next render instruction is posted
        osMessageQueueGet(hm10RenderInstructionQueue, &packet, NULL, osWaitForever);

        switch (packet.widget_state)
        {
        case RENDER_WIDGET_STATE_ACTIVE:
            DebugTask_RenderPhoneFrame(&packet.hm10_packet);
            break;

        case RENDER_WIDGET_STATE_CONNECTED:
            DebugTerminal_PrintLine(&huart3, "BLE: connection established");
            break;

        case RENDER_WIDGET_STATE_IDLE:
        default:
            DebugTerminal_PrintLine(&huart3, "BLE: connection terminated");
            DebugTask_ClearPhoneRenderArea();

            UTIL_LCD_SetFont(&Font16);
            UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
            UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
            UTIL_LCD_DisplayStringAt((phone_render_bounds.x_max + phone_render_bounds.x_min) / 2U - 35U,
                                     (phone_render_bounds.y_max + phone_render_bounds.y_min) / 2U,
                                     (uint8_t*) "WAITING",
                                     LEFT_MODE);
            break;
        }
    }
}

/**
 * @brief The core thread.
 * @param argument Does not impact the result.
 * @return None.
 */
void MainThread(void* argument)
{
    uint32_t last_mpu6050_tick = HAL_GetTick() - MPU6050_DEBUG_UPDATE_PERIOD_MS;

    DebugTerminal_PrintMode(&huart3, debug_terminal_mode);

    while (1)
    {
        DebugTerminal_HandleInput(&huart3, &debug_terminal_mode);

        if (debug_terminal_mode == DEBUG_TERMINAL_MODE_PINGS)
        {
            DebugTask_RunPingSequence(&debug_terminal_mode);
            osDelay(1U);
            continue;
        }

        if (debug_terminal_mode == DEBUG_TERMINAL_MODE_MPU6050_DATA)
        {
            DebugTask_PrintMpu6050Data(&last_mpu6050_tick);
        }

        osDelay(1U);
    }
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

    /* LCD Init ----------------------------------------------------------------*/
    if (BSP_LCD_Init(TRAIL_HUD_LCD_INSTANCE, LCD_ORIENTATION_LANDSCAPE) != BSP_ERROR_NONE)
    {
        Error_Handler();
    }

    BSP_LCD_DisplayOn(TRAIL_HUD_LCD_INSTANCE);
    BSP_LCD_SetBrightness(TRAIL_HUD_LCD_INSTANCE, 100U);
    BSP_LCD_SetActiveLayer(TRAIL_HUD_LCD_INSTANCE, 0U);
    UTIL_LCD_SetFuncDriver(&LCD_Driver);
    UTIL_LCD_SetLayer(0U);
    TrailGui_DrawLoadingScreen(TRAIL_HUD_LOADING_STAGE_COUNT);

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized LCD");
    TrailGui_ExpandLoadingBar(1U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* HM-10 Init --------------------------------------------------------------*/
    if (HM10_Init(&hm10, &huart1) != HM10_OK)
    {
        Error_Handler();
    }

    if (HM10_SetNameAndReset(&hm10, "TRAIL-HUD", 1000U) != HM10_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized HM-10 on USART1");
    TrailGui_ExpandLoadingBar(2U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* MPU6050 Init ------------------------------------------------------------*/
    if (MPU6050_Init(&mpu6050, &hi2c4, MPU6050_DEFAULT_I2C_ADDRESS) != MPU6050_OK)
    {
        Error_Handler();
    }

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized MPU-6050 on I2C4");
    TrailGui_ExpandLoadingBar(3U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* LED Init ----------------------------------------------------------------*/
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    for (uint16_t led = 0U; led < LED_HANDLE_COUNT; led++)
    {
        GPIO_InitStruct.Pin = led_handles[led].GPIO_Pin;
        HAL_GPIO_Init(led_handles[led].GPIO_Port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(led_handles[led].GPIO_Port, led_handles[led].GPIO_Pin, led_handles[led].GPIO_DefaultState);
    }

    LED_ToggleSequence(450U);

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized LED");
    TrailGui_ExpandLoadingBar(4U, TRAIL_HUD_LOADING_STAGE_COUNT);
    HAL_Delay(1000U);

    TrailGui_DrawDefaultScreen();
    TrailGui_DrawBoundingRectangle(phone_render_padding_bounds, 10U, UTIL_LCD_COLOR_WHITE);
    TrailGui_DrawBoundingRectangle(phone_gps_padding_bounds, 10U, UTIL_LCD_COLOR_WHITE);
    /* USER CODE END 2 */

    /* Init scheduler */
    osKernelInitialize();

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    hm10TopLED = osSemaphoreNew(1, 0, NULL);
    hm10BottomLED = osSemaphoreNew(1, 0, NULL);
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    hm10RenderInstructionQueue = osMessageQueueNew(16, sizeof(TrailGui_RenderWidgetPacket), NULL);
    HAL_UART_Receive_IT(&huart1, &hm10_uart_rx_byte, 1U);

    hm10_connection_state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);
    DebugTerminal_PrintLine(&huart3,
                            (hm10_connection_state == GPIO_PIN_SET)
                                ? "BLE: connection established"
                                : "BLE: connection terminated");

    TrailGui_RenderWidgetPacket initial_render_packet;
    initial_render_packet.widget_state = (hm10_connection_state == GPIO_PIN_SET)
                                              ? RENDER_WIDGET_STATE_CONNECTED
                                              : RENDER_WIDGET_STATE_IDLE;
    osMessageQueuePut(hm10RenderInstructionQueue, &initial_render_packet, 0U, 0U);

    HAL_NVIC_SetPriority(EXTI3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    /* USER CODE END RTOS_QUEUES */

    /* USER CODE BEGIN RTOS_THREADS */
    osThreadNew(HM10_TopLEDThread, NULL, NULL);
    osThreadNew(HM10_BottomLEDThread, NULL, NULL);
    osThreadNew(HM10_Thread, NULL, &HM10Thread_attributes);
    osThreadNew(MainThread, NULL, &MainThread_attributes);
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

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
        RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
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

    /** Configure Analogue filter */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Digital filter */
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
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HM10_STATE_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief Re-arms USART1 byte reception after a UART error.
 * @param huart UART handle reporting the error; only USART1 is handled.
 * @return None.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1)
    {
        HAL_UART_Receive_IT(&huart1, &hm10_uart_rx_byte, 1U);
    }
}

/**
 * @brief Assembles and dispatches one line received on the HM-10 UART.
 * @param huart UART handle reporting the completed byte reception; only
 *              USART1 is handled.
 * @return None.
 *
 * Runs in interrupt context, one byte per call. '\r' is ignored, '\n'
 * terminates and dispatches the accumulated line, any other byte is
 * appended to hm10_isr_line. A completed ping-reply line releases
 * hm10BottomLED and sets hm10_ping_reply_flag. A completed data packet is
 * parsed and, on success, copied *by value* into a TrailGui_RenderWidgetPacket
 * message and posted to hm10RenderInstructionQueue so HM10_Thread can render
 * it outside of interrupt context.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1)
    {
        uint8_t byte = hm10_uart_rx_byte;

        // Re-arm immediately so no bytes are missed
        HAL_UART_Receive_IT(&huart1, &hm10_uart_rx_byte, 1U);

        if (byte == '\r')
        {
            return;
        }

        if (byte == '\n')
        {
            hm10_isr_line[hm10_isr_line_len] = '\0';

            if (hm10_isr_line_len > 0U)
            {
                if (strcmp(hm10_isr_line, DEBUG_TERMINAL_PING_REPLY) == 0)
                {
                    osSemaphoreRelease(hm10BottomLED);
                    hm10_ping_reply_flag = 1U;
                }
                else
                {
                    osSemaphoreRelease(hm10TopLED);
                    HM10_DataPacket data_packet;

                    if (HM10_ParseDataPacket(hm10_isr_line, &data_packet) != 0U)
                    {
                        TrailGui_RenderWidgetPacket render_packet;
                        render_packet.hm10_packet = data_packet;   /* copy by value into the message */
                        render_packet.widget_state = RENDER_WIDGET_STATE_ACTIVE;
                        osMessageQueuePut(hm10RenderInstructionQueue, &render_packet, 0U, 0U);

                        if (debug_terminal_mode == DEBUG_TERMINAL_MODE_HM10_DATA)
                        {
                            DebugTerminal_ParsePhonePacket(&huart3, hm10_isr_line);
                        }
                    }
                }
            }

            hm10_isr_line_len = 0U;
            hm10_isr_line[0] = '\0';
            return;
        }

        if (hm10_isr_line_len < (BLE_RX_LINE_SIZE - 1U))
        {
            hm10_isr_line[hm10_isr_line_len++] = (char)byte;
        }
        else
        {
            hm10_isr_line_len = 0U;
            hm10_isr_line[0] = '\0';
        }
    }
}

/**
 * @brief Updates the cached HM-10 connection state and notifies HM10_Thread.
 * @param GPIO_Pin Pin number that triggered the EXTI line; only
 *                 HM10_STATE_Pin is handled.
 * @return None.
 *
 * Fires on both edges of HM10_STATE_Pin. This is the only interrupt-time
 * caller of HAL_GPIO_ReadPin() for this pin; every other consumer reads the
 * cached hm10_connection_state value instead. Posts a lightweight
 * RENDER_WIDGET_STATE_CONNECTED or RENDER_WIDGET_STATE_IDLE message so
 * HM10_Thread performs the actual LCD/debug-terminal update outside of
 * interrupt context.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == HM10_STATE_Pin)
    {
        hm10_connection_state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);

        TrailGui_RenderWidgetPacket render_packet;
        render_packet.widget_state = (hm10_connection_state == GPIO_PIN_SET)
                                          ? RENDER_WIDGET_STATE_CONNECTED
                                          : RENDER_WIDGET_STATE_IDLE;
        osMessageQueuePut(hm10RenderInstructionQueue, &render_packet, 0U, 0U);
    }
}

/* USER CODE END 4 */

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
