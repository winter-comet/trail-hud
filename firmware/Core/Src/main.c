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
  * HM-10 HM-10 AT-09 EN -> STM32H750B-DK PE3
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

#include "task.h"

#include <malloc.h>
#include <math.h>
#include <stdio.h>
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

/**
 * @brief Lists the tilt alert stages reported to the phone over BLE.
 *
 * Values:
 * - TILT_ALERT_STATE_CLEAR: Tilt is under the warning threshold. The phone is
 *   not vibrating and a fresh warning crossing may fire again.
 * - TILT_ALERT_STATE_WARNING: Tilt crossed the warning threshold, so the phone
 *   was asked for one single vibration. Held so that staying tilted does not
 *   repeat the buzz on every poll.
 * - TILT_ALERT_STATE_CRITICAL: Tilt crossed the critical threshold and the
 *   phone is vibrating continuously. Only falling back under the warning
 *   threshold releases it, not merely dropping under the critical one.
 */
typedef enum
{
    TILT_ALERT_STATE_CLEAR = 0,
    TILT_ALERT_STATE_WARNING,
    TILT_ALERT_STATE_CRITICAL,
} TrailHud_TiltAlertState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Board and system --------------------------------------------------------*/
#define TRAIL_HUD_LCD_INSTANCE 0U
#define TRAIL_HUD_LOADING_STAGE_COUNT 4U
#define LED_HANDLE_COUNT 3U
#define MPU6050_DEBUG_UPDATE_PERIOD_MS 1000U
#define HM10_DEBUG_UPDATE_PERIOD_MS 1000U
#define HM10_DROP_REPORT_PERIOD_MS 2000U

/* Phone render ------------------------------------------------------------*/
#define PHONE_RENDER_MARGIN 6U
#define PHONE_RENDER_PADDING 12U
#define PHONE_RENDER_LINE_WIDTH 1U
#define PHONE_RENDER_LINE_COLOR UTIL_LCD_COLOR_WHITE
#define PHONE_RENDER_CLEAR_COLOR UTIL_LCD_COLOR_BLACK

/* Tilt indicator render ---------------------------------------------------*/
#define TILT_RENDER_MARGIN 6U
#define TILT_RENDER_UPDATE_PERIOD_MS 500U
#define TILT_SAMPLE_PERIOD_MS 50U
#define TILT_FILTER_ALPHA 0.25f
#define TILT_RENDER_LINE_WIDTH TRAIL_GUI_LINE_WIDTH_THIN
#define TILT_RENDER_LINE_COLOR UTIL_LCD_COLOR_BLACK
#define TILT_RENDER_CLEAR_COLOR UTIL_LCD_COLOR_WHITE

/* Tilt alert --------------------------------------------------------------*/
#define TILT_ALERT_WARNING_DEG 20.0f
#define TILT_ALERT_CRITICAL_DEG 25.0f
#define TILT_ALERT_PACKET_SINGLE "trailhud:vibrate:once"
#define TILT_ALERT_PACKET_CONTINUOUS "trailhud:vibrate:start"
#define TILT_ALERT_PACKET_CLEAR "trailhud:vibrate:stop"
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

/* HM-10 USART1 RX ISR state. These belong here, not in hm10.h, because only
 * HAL_UART_RxCpltCallback/HAL_UART_ErrorCallback below ever touch them. */
static uint8_t hm10_uart_rx_byte;
static uint16_t hm10_isr_line_len = 0U;
static char hm10_isr_line[HM10_DEFAULT_LINE_SIZE];

/* Set once a line outgrows hm10_isr_line, and cleared at the next newline.
 * Without it the tail of an over-long line would be parsed as a fresh packet. */
static uint8_t hm10_isr_line_overflow = 0U;

/* Counts render messages the ISR could not queue. Surfaced by MainThread so
 * backpressure shows up on the terminal instead of silently losing frames. */
static volatile uint32_t hm10_render_queue_drops = 0U;

osSemaphoreId_t hm10_top_led_semaphore;
osSemaphoreId_t hm10_bottom_led_semaphore;
osMessageQueueId_t hm10_render_queue;

static volatile uint8_t hm10_ping_reply_flag = 0U;
static volatile DebugTerminalMode debug_terminal_mode = DEBUG_TERMINAL_MODE_WAITING;
static volatile GPIO_PinState hm10_connection_state = GPIO_PIN_RESET;
static TrailHud_TiltAlertState tilt_alert_state = TILT_ALERT_STATE_CLEAR;

/* Set once the LCD is far enough up that TrailHud_Fatal can draw on it. */
static uint8_t trail_hud_lcd_ready = 0U;

/* Rolling average of the tilt samples, held as a packet so it can be handed
 * straight to the renderer. Primed by the first successful read. */
static MPU6050_DataPacket tilt_filtered_packet;
static uint8_t tilt_filter_primed = 0U;

/* Latches the one-shot warning about packets arriving while HM10_STATE is low. */
static uint8_t hm10_state_mismatch_reported = 0U;

static const LED_HandleTypeDef led_handles[LED_HANDLE_COUNT] = {
    {GPIOD, GPIO_PIN_3, GPIO_PIN_RESET},
    {GPIOJ, GPIO_PIN_2, GPIO_PIN_SET},
    {GPIOI, GPIO_PIN_13, GPIO_PIN_SET},
};
static const TrailGui_BoundingBox phone_render_bounds = {
    .x_min = 241U + (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING),
    .x_max = 479U - (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING),
    .y_min = 0U + (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING),
    .y_max = 239U - (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING),
};
static const TrailGui_BoundingBox phone_render_padding_bounds = {
    .x_min = phone_render_bounds.x_min - PHONE_RENDER_PADDING,
    .x_max = phone_render_bounds.x_max + PHONE_RENDER_PADDING,
    .y_min = phone_render_bounds.y_min - PHONE_RENDER_PADDING,
    .y_max = phone_render_bounds.y_max + PHONE_RENDER_PADDING,
};
static const TrailGui_BoundingBox phone_gps_bounds = {
    .x_min = phone_render_bounds.x_min,
    .x_max = phone_render_bounds.x_max,
    .y_min = phone_render_padding_bounds.y_max + (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING / 2),
    .y_max = 272U - (PHONE_RENDER_MARGIN + PHONE_RENDER_PADDING / 2),
};
static const TrailGui_BoundingBox phone_gps_padding_bounds = {
    .x_min = phone_gps_bounds.x_min - PHONE_RENDER_PADDING,
    .x_max = phone_gps_bounds.x_max + PHONE_RENDER_PADDING,
    .y_min = phone_gps_bounds.y_min - PHONE_RENDER_PADDING / 2,
    .y_max = phone_gps_bounds.y_max + PHONE_RENDER_PADDING / 2,
};
/* Inset from the gyroscope panel so the clear pass never paints over its
 * rounded corners, and so the widget stays centered on the panel itself. */
static const TrailGui_BoundingBox tilt_render_bounds = {
    .x_min = TRAIL_GUI_GYROSCOPE_BACKGROUND_X_MIN + TILT_RENDER_MARGIN,
    .x_max = TRAIL_GUI_GYROSCOPE_BACKGROUND_X_MAX - TILT_RENDER_MARGIN,
    .y_min = TRAIL_GUI_GYROSCOPE_BACKGROUND_Y_MIN + TILT_RENDER_MARGIN,
    .y_max = TRAIL_GUI_GYROSCOPE_BACKGROUND_Y_MAX - TILT_RENDER_MARGIN,
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
/* Board helpers */
static void LED_ToggleSequence(uint32_t delay_ms);
static void TrailHud_Fatal(const char* reason);

/* Tilt alerts */
static void TrailHud_UpdateTiltAlert(float tilt_deg);

/* LCD rendering */
static void TrailHud_ClearPhoneRenderArea(void);
static void TrailHud_RenderPhoneFrame(const HM10_DataPacket* hm10_packet);
static void TrailHud_SampleTilt(uint32_t* last_tick);
static void TrailHud_RenderTiltFrame(void);
static void TrailHud_UpdateTiltFrame(uint32_t* last_tick);

/* Debug terminal */
static uint8_t DebugTask_WaitForPingReply(uint32_t timeout_ms);
static void DebugTask_RunPingSequence(volatile DebugTerminalMode* debug_mode);
static void DebugTask_PrintMpu6050Data(uint32_t* last_tick);
static void DebugTask_ReportQueueDrops(uint32_t* reported_drops, uint32_t* last_tick);

/* RTOS threads */
void HM10_TopLEDThread(void* argument);
void HM10_BottomLEDThread(void* argument);
void HM10_Thread(void* argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Reports an unrecoverable start-up failure, then halts.
 * @param reason Short description of what failed; NULL is allowed and is
 *               printed as "(null)".
 * @return Never returns.
 *
 * Error_Handler() disables interrupts and spins, so on its own a failed sensor
 * leaves the loading bar frozen with nothing to explain it. Reporting has to
 * happen before that: the debug UART is brought up before the LCD and both
 * modules, so it can describe every failure the board can survive long enough
 * to notice, and the LCD carries the same text once it is available.
 */
static void TrailHud_Fatal(const char* reason)
{
    DebugTerminal_PrintLine(&huart3, reason);

    if (trail_hud_lcd_ready != 0U)
    {
        UTIL_LCD_SetFont(&Font12);
        UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_RED);
        UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
        UTIL_LCD_DisplayStringAt(TRAIL_GUI_SCREEN_MARGIN,
                                 TRAIL_GUI_SCREEN_HEIGHT - 20U,
                                 (uint8_t*)((reason != NULL) ? reason : "(null)"),
                                 LEFT_MODE);
    }

    Error_Handler();
}

/**
 * @brief Toggles a LED sequence, toggling all pins, defined in the led_handles array.
 * @param delay_ms Delay in milliseconds held after each individual LED
 *                 transition. Every LED is switched on for delay_ms and then
 *                 off for delay_ms before the next one starts, so the whole
 *                 sequence takes 2 * LED_HANDLE_COUNT * delay_ms.
 * @return None.
 */
static void LED_ToggleSequence(uint32_t delay_ms)
{
    for (uint16_t led = 0U; led < LED_HANDLE_COUNT; led++)
    {
        HAL_GPIO_TogglePin(led_handles[led].GPIO_Port, led_handles[led].GPIO_Pin);
        HAL_Delay(delay_ms);
        HAL_GPIO_TogglePin(led_handles[led].GPIO_Port, led_handles[led].GPIO_Pin);
        HAL_Delay(delay_ms);
    }
}

/**
 * @brief Drives the phone vibration alert from one tilt angle.
 * @param tilt_deg Tilt angle in degrees as reported by
 *                 TrailGui_TiltAngleFromAccelerometer. The sign only selects
 *                 the direction of the lean, so the thresholds are compared
 *                 against its magnitude and fire either way.
 * @return None.
 *
 * Sends a packet only on a stage change, so a steady lean costs one BLE write
 * rather than one per poll. Crossing TILT_ALERT_WARNING_DEG asks for a single
 * buzz; crossing TILT_ALERT_CRITICAL_DEG asks for continuous vibration that is
 * released only once the tilt falls back under TILT_ALERT_WARNING_DEG, so the
 * two thresholds form a hysteresis band that will not chatter around 25
 * degrees. Nothing is sent while the BLE link is down.
 */
static void TrailHud_UpdateTiltAlert(float tilt_deg)
{
    float tilt_magnitude_deg = fabsf(tilt_deg);
    const char* alert_packet = NULL;

    if (hm10_connection_state != GPIO_PIN_SET)
    {
        /* Nothing can be delivered, and the phone drops its own vibration when
         * the link goes away, so re-arm from a clean slate for the next one. */
        tilt_alert_state = TILT_ALERT_STATE_CLEAR;
        return;
    }

    if (tilt_alert_state == TILT_ALERT_STATE_CRITICAL)
    {
        if (tilt_magnitude_deg < TILT_ALERT_WARNING_DEG)
        {
            tilt_alert_state = TILT_ALERT_STATE_CLEAR;
            alert_packet = TILT_ALERT_PACKET_CLEAR "\r\n";
        }
    }
    else if (tilt_magnitude_deg >= TILT_ALERT_CRITICAL_DEG)
    {
        tilt_alert_state = TILT_ALERT_STATE_CRITICAL;
        alert_packet = TILT_ALERT_PACKET_CONTINUOUS "\r\n";
    }
    else if (tilt_magnitude_deg >= TILT_ALERT_WARNING_DEG)
    {
        if (tilt_alert_state == TILT_ALERT_STATE_CLEAR)
        {
            tilt_alert_state = TILT_ALERT_STATE_WARNING;
            alert_packet = TILT_ALERT_PACKET_SINGLE "\r\n";
        }
    }
    else
    {
        tilt_alert_state = TILT_ALERT_STATE_CLEAR;
    }

    if (alert_packet != NULL)
    {
        (void)HM10_SendString(&hm10, alert_packet);
    }
}

/**
 * @brief Fills the phone render area with the solid clear color.
 * @return None.
 */
static void TrailHud_ClearPhoneRenderArea(void)
{
    TrailGui_DrawRoundedRectangle(phone_render_bounds, 0U, PHONE_RENDER_CLEAR_COLOR);
    TrailGui_DrawRoundedRectangle(phone_gps_bounds, 0U, PHONE_RENDER_CLEAR_COLOR);
}

/**
 * @brief Redraws the phone orientation cuboid for one parsed HM-10 packet.
 * @param hm10_packet Parsed HM-10 data packet holding the phone orientation
 *                    quaternion fields; NULL is not allowed.
 * @return None.
 *
 * Deliberately not gated on hm10_connection_state. A packet that arrived and
 * parsed is proof the link is up, whatever the STATE pin reads, so gating here
 * could only ever discard data the board actually received.
 */
static void TrailHud_RenderPhoneFrame(const HM10_DataPacket* hm10_packet)
{
    if (hm10_packet == NULL)
    {
        return;
    }

    TrailHud_ClearPhoneRenderArea();
    TrailGui_RenderPhoneCuboid(hm10_packet,
                             phone_render_bounds,
                             PHONE_RENDER_LINE_WIDTH,
                             PHONE_RENDER_LINE_COLOR);
    TrailGui_RenderPhoneGps(hm10_packet, phone_gps_bounds, UTIL_LCD_COLOR_WHITE);
}

/**
 * @brief Samples the MPU-6050 and folds the reading into the tilt average.
 * @param last_tick Pointer to the HAL tick value recorded at the last sample.
 *                  NULL is not allowed. Updated on every attempt, whether the
 *                  sensor read succeeds or fails.
 * @return None.
 *
 * Sampling runs far faster than the widget redraws so the displayed angle is
 * an average rather than whatever single instant the redraw happened to land
 * on. The average is taken over the accelerometer components rather than the
 * derived angle: an exponential average across the +/-180 degree wrap would
 * drag the needle the long way round through the discontinuity.
 */
static void TrailHud_SampleTilt(uint32_t* last_tick)
{
    MPU6050_DataPacket packet;

    if ((HAL_GetTick() - *last_tick) < TILT_SAMPLE_PERIOD_MS)
    {
        return;
    }

    *last_tick = HAL_GetTick();

    if (MPU6050_ReadDataPacket(&mpu6050, &packet) != MPU6050_OK)
    {
        return;
    }

    if (tilt_filter_primed != 0U)
    {
        packet.accel_x_g = tilt_filtered_packet.accel_x_g +
            (TILT_FILTER_ALPHA * (packet.accel_x_g - tilt_filtered_packet.accel_x_g));
        packet.accel_y_g = tilt_filtered_packet.accel_y_g +
            (TILT_FILTER_ALPHA * (packet.accel_y_g - tilt_filtered_packet.accel_y_g));
    }

    tilt_filtered_packet = packet;
    tilt_filter_primed = 1U;
}

/**
 * @brief Redraws the tilt indicator widget from one fresh MPU-6050 sample.
 * @return None.
 *
 * The previous frame is erased before the new one is drawn, so the widget area
 * is left filled with TILT_RENDER_CLEAR_COLOR and never accumulates leftovers
 * from an earlier angle. A failed sensor read leaves the last drawn frame on
 * the LCD instead of blanking the widget. The same sample also drives the
 * phone vibration alert, so the alert runs at the redraw rate.
 */
static void TrailHud_RenderTiltFrame(void)
{
    if (tilt_filter_primed == 0U)
    {
        return;
    }

    TrailGui_DrawRoundedRectangle(tilt_render_bounds, 0U, TILT_RENDER_CLEAR_COLOR);
    TrailGui_RenderTiltIndicator(&tilt_filtered_packet,
                                 tilt_render_bounds,
                                 TILT_RENDER_LINE_WIDTH,
                                 TILT_RENDER_LINE_COLOR,
                                 TILT_RENDER_CLEAR_COLOR);
    TrailHud_UpdateTiltAlert(TrailGui_TiltAngleFromAccelerometer(&tilt_filtered_packet));
}

/**
 * @brief Redraws the tilt indicator widget at a fixed periodic interval.
 * @param last_tick Pointer to the HAL tick value recorded at the last redraw.
 *                  NULL is not allowed. Updated to the current tick after each
 *                  redraw attempt, whether the sensor read succeeds or fails.
 * @return None.
 */
static void TrailHud_UpdateTiltFrame(uint32_t* last_tick)
{
    if ((HAL_GetTick() - *last_tick) < TILT_RENDER_UPDATE_PERIOD_MS)
    {
        return;
    }

    TrailHud_RenderTiltFrame();

    *last_tick = HAL_GetTick();
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

    /* The caller arms hm10_ping_reply_flag before it transmits. Clearing it
     * here would discard a reply that had already arrived. */
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
static void DebugTask_RunPingSequence(volatile DebugTerminalMode* debug_mode)
{
    DebugTerminal_PrintLine(&huart3, "BLE: pinging phone with 4 packets of data");

    for (uint8_t ping_index = 0U; ping_index < DEBUG_TERMINAL_PING_COUNT; ping_index++)
    {
        uint8_t reply_received = 0U;

        if (hm10_connection_state == GPIO_PIN_SET)
        {
            /* Armed before transmitting: a reply that lands while the ping is
             * still on the wire would otherwise be cleared by the waiter and
             * reported as no reply at all. */
            hm10_ping_reply_flag = 0U;

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
 * @brief Reports newly dropped render messages on the debug terminal.
 * @param reported_drops Pointer to the drop total already printed; NULL is not
 *                       allowed. Updated once the current total is reported.
 * @param last_tick Pointer to the HAL tick value recorded at the last report.
 *                  NULL is not allowed. Updated after each report.
 * @return None.
 *
 * The producers run in interrupt context and cannot block, so a full render
 * queue silently loses frames. Printing the running total turns that into
 * something the terminal states plainly instead of an unexplained gap in the
 * rendering. The interval matters as much as the count: a queue that stays
 * full would otherwise generate a report per loop pass and saturate the
 * 9600-baud debug link.
 */
static void DebugTask_ReportQueueDrops(uint32_t* reported_drops, uint32_t* last_tick)
{
    char text[48];
    uint32_t drops;

    if ((HAL_GetTick() - *last_tick) < HM10_DROP_REPORT_PERIOD_MS)
    {
        return;
    }

    drops = hm10_render_queue_drops;

    if (drops == *reported_drops)
    {
        return;
    }

    *reported_drops = drops;
    *last_tick = HAL_GetTick();

    snprintf(text, sizeof(text), "BLE: dropped %lu render packets", (unsigned long)drops);
    DebugTerminal_PrintLine(&huart3, text);
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

    if ((HAL_GetTick() - *last_tick) < MPU6050_DEBUG_UPDATE_PERIOD_MS)
    {
        return;
    }

    MPU6050_StatusTypeDef status = MPU6050_ReadDataPacket(&mpu6050, &packet);

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
        osSemaphoreAcquire(hm10_top_led_semaphore, osWaitForever);
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
        osSemaphoreAcquire(hm10_bottom_led_semaphore, osWaitForever);
        HAL_GPIO_TogglePin(GPIOJ, GPIO_PIN_2);
        osDelay(100);
        HAL_GPIO_TogglePin(GPIOJ, GPIO_PIN_2);
    }
}

/**
 * @brief Parses queued BLE lines and applies each message to the LCD/debug terminal.
 * @param argument Does not impact the result.
 * @return None.
 *
 * Phone packets arrive as raw text and are parsed here, in task context. The
 * PHONE DATA terminal output is emitted from this thread too, throttled to
 * HM10_DEBUG_UPDATE_PERIOD_MS: one formatted line costs roughly 150ms of
 * transmit time at 9600 baud, so an unthrottled BLE stream would queue up
 * faster than the debug UART can drain it.
 */
void HM10_Thread(void* argument)
{
    TrailGui_RenderWidgetPacket packet;
    uint32_t last_phone_print_tick = HAL_GetTick() - HM10_DEBUG_UPDATE_PERIOD_MS;

    while (1)
    {
        // Wait until the next render instruction is posted
        osMessageQueueGet(hm10_render_queue, &packet, NULL, osWaitForever);

        switch (packet.widget_state)
        {
        case RENDER_WIDGET_STATE_ACTIVE:
        {
            HM10_DataPacket data_packet;

            /* Parsed here rather than in the USART1 ISR. HM10_ParseDataPacket
             * runs eight strtod calls, and strtod reaches _Balloc and the libc
             * malloc behind it, which this build leaves unguarded. Task context
             * also gives it a 4KB stack instead of the 1KB main stack that every
             * interrupt shares. */
            if (HM10_ParseDataPacket(packet.line, &data_packet) == 0U)
            {
                break;
            }

            if ((hm10_connection_state != GPIO_PIN_SET) &&
                (hm10_state_mismatch_reported == 0U))
            {
                hm10_state_mismatch_reported = 1U;
                DebugTerminal_PrintLine(&huart3,
                                        "BLE: packets arriving while HM10_STATE reads low, check PG3");
            }

            TrailHud_RenderPhoneFrame(&data_packet);

            if ((debug_terminal_mode == DEBUG_TERMINAL_MODE_HM10_DATA) &&
                ((HAL_GetTick() - last_phone_print_tick) >= HM10_DEBUG_UPDATE_PERIOD_MS))
            {
                DebugTerminal_PrintPhonePacket(&huart3, &data_packet);
                last_phone_print_tick = HAL_GetTick();
            }

            break;
        }

        case RENDER_WIDGET_STATE_CONNECTED:
            DebugTerminal_PrintLine(&huart3, "BLE: connection established");
            break;

        case RENDER_WIDGET_STATE_IDLE:
        default:
            DebugTerminal_PrintLine(&huart3, "BLE: connection terminated");
            TrailHud_ClearPhoneRenderArea();

            UTIL_LCD_SetFont(&Font12);
            UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
            UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
            UTIL_LCD_DisplayStringAt((phone_render_bounds.x_max + phone_render_bounds.x_min) / 2U - 25U,
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
    uint32_t last_tilt_tick = HAL_GetTick();
    uint32_t last_tilt_sample_tick = HAL_GetTick();
    uint32_t last_drop_report_tick = HAL_GetTick();
    uint32_t reported_queue_drops = 0U;

    DebugTerminal_PrintMode(&huart3, debug_terminal_mode);

    while (1)
    {
        DebugTerminal_HandleInput(&huart3, &debug_terminal_mode);
        TrailHud_SampleTilt(&last_tilt_sample_tick);
        TrailHud_UpdateTiltFrame(&last_tilt_tick);
        DebugTask_ReportQueueDrops(&reported_queue_drops, &last_drop_report_tick);

        /* PHONE DATA has no case here: the packets are parsed and printed by
         * HM10_Thread as they arrive, so there is nothing to poll for. */
        switch (debug_terminal_mode)
        {
        case DEBUG_TERMINAL_MODE_PINGS:
            DebugTask_RunPingSequence(&debug_terminal_mode);
            break;
        case DEBUG_TERMINAL_MODE_MPU6050_DATA:
            DebugTask_PrintMpu6050Data(&last_mpu6050_tick);
            break;
        default:
            break;
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
        TrailHud_Fatal("FATAL: LCD init failed");
    }

    BSP_LCD_DisplayOn(TRAIL_HUD_LCD_INSTANCE);
    BSP_LCD_SetBrightness(TRAIL_HUD_LCD_INSTANCE, 100U);
    BSP_LCD_SetActiveLayer(TRAIL_HUD_LCD_INSTANCE, 0U);
    UTIL_LCD_SetFuncDriver(&LCD_Driver);
    UTIL_LCD_SetLayer(0U);
    trail_hud_lcd_ready = 1U;
    TrailGui_DrawLoadingScreen(TRAIL_HUD_LOADING_STAGE_COUNT);

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized LCD");
    TrailGui_ExpandLoadingBar(1U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* HM-10 Init --------------------------------------------------------------*/
    if (HM10_Init(&hm10, &huart1) != HM10_OK)
    {
        TrailHud_Fatal("FATAL: HM-10 handle init failed");
    }

    if (HM10_SetNameAndReset(&hm10, "TRAIL-HUD", 1000U) != HM10_OK)
    {
        TrailHud_Fatal("FATAL: HM-10 not responding on USART1");
    }

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* Armed here rather than alongside the RTOS objects, which is several
     * seconds of LED sequencing and sensor start-up later; anything the phone
     * sent in that window used to be lost. The receive interrupt only touches
     * the queue and semaphores through NULL checks, so it is safe this early.
     * Clearing the overrun flag and reading RDR first discards the byte the
     * module's own reset reply leaves sitting in the register, which would
     * otherwise become the first character of the first line. */
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    (void)huart1.Instance->RDR;
    HAL_UART_Receive_IT(&huart1, &hm10_uart_rx_byte, 1U);

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized HM-10 on USART1");
    TrailGui_ExpandLoadingBar(2U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* MPU6050 Init ------------------------------------------------------------*/
    if (MPU6050_Init(&mpu6050, &hi2c4, MPU6050_DEFAULT_I2C_ADDRESS) != MPU6050_OK)
    {
        TrailHud_Fatal("FATAL: MPU-6050 not responding on I2C4");
    }

    DebugTerminal_PrintLine(&huart3, "DEBUG: initialized MPU-6050 on I2C4");
    TrailGui_ExpandLoadingBar(3U, TRAIL_HUD_LOADING_STAGE_COUNT);

    /* LED Init ----------------------------------------------------------------*/
    /* GPIOD is already clocked by MX_GPIO_Init(). */
    __HAL_RCC_GPIOC_CLK_ENABLE();
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

    uint32_t tilt_prime_tick = HAL_GetTick() - TILT_SAMPLE_PERIOD_MS;
    TrailHud_SampleTilt(&tilt_prime_tick);
    TrailHud_RenderTiltFrame();
    TrailGui_DrawBoundingRectangle(phone_render_padding_bounds, 10U, UTIL_LCD_COLOR_WHITE);
    TrailGui_DrawBoundingRectangle(phone_gps_padding_bounds, 10U, UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_SetFont(&Font12);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_DisplayStringAt((phone_render_padding_bounds.x_max + phone_render_padding_bounds.x_min) / 2U - 45U,
                             phone_render_padding_bounds.y_min + 2U,
                             (uint8_t*) "DEVICE RENDER",
                             LEFT_MODE);
    /* USER CODE END 2 */

    /* Init scheduler */
    osKernelInitialize();
    DebugTerminal_Init();

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    hm10_top_led_semaphore = osSemaphoreNew(1, 0, NULL);
    hm10_bottom_led_semaphore = osSemaphoreNew(1, 0, NULL);

    if ((hm10_top_led_semaphore == NULL) || (hm10_bottom_led_semaphore == NULL))
    {
        TrailHud_Fatal("FATAL: LED semaphore allocation failed");
    }
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    hm10_render_queue = osMessageQueueNew(16, sizeof(TrailGui_RenderWidgetPacket), NULL);

    if (hm10_render_queue == NULL)
    {
        TrailHud_Fatal("FATAL: render queue allocation failed");
    }

    hm10_connection_state = HAL_GPIO_ReadPin(HM10_STATE_GPIO_Port, HM10_STATE_Pin);
    DebugTerminal_PrintLine(&huart3,
                            (hm10_connection_state == GPIO_PIN_SET)
                                ? "BLE: connection established"
                                : "BLE: connection terminated");

    TrailGui_RenderWidgetPacket initial_render_packet = {0};
    initial_render_packet.widget_state = (hm10_connection_state == GPIO_PIN_SET)
                                              ? RENDER_WIDGET_STATE_CONNECTED
                                              : RENDER_WIDGET_STATE_IDLE;
    osMessageQueuePut(hm10_render_queue, &initial_render_packet, 0U, 0U);

    HAL_NVIC_SetPriority(EXTI3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    /* USER CODE END RTOS_QUEUES */

    /* USER CODE BEGIN RTOS_THREADS */
    if ((osThreadNew(HM10_TopLEDThread, NULL, NULL) == NULL) ||
        (osThreadNew(HM10_BottomLEDThread, NULL, NULL) == NULL) ||
        (osThreadNew(HM10_Thread, NULL, &HM10Thread_attributes) == NULL) ||
        (osThreadNew(MainThread, NULL, &MainThread_attributes) == NULL))
    {
        TrailHud_Fatal("FATAL: thread allocation failed");
    }
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
 * @brief Serialises newlib heap access against the FreeRTOS scheduler.
 * @param reent Reentrancy structure supplied by newlib; not used.
 * @return None.
 *
 * newlib ships __retarget_lock_acquire_recursive as an empty stub, so the libc
 * heap had no mutual exclusion at all. It is reachable from more places than it
 * looks: strtod calls _Balloc for its bigint path, and snprintf can allocate
 * too, so two threads could interleave inside malloc and corrupt the heap.
 * Suspending the scheduler is sufficient only because the BLE receive path no
 * longer parses in interrupt context; nothing may call this from an ISR.
 */
void __malloc_lock(struct _reent* reent)
{
    (void)reent;
    vTaskSuspendAll();
}

/**
 * @brief Releases the newlib heap lock taken by __malloc_lock.
 * @param reent Reentrancy structure supplied by newlib; not used.
 * @return None.
 */
void __malloc_unlock(struct _reent* reent)
{
    (void)reent;
    (void)xTaskResumeAll();
}

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

        HAL_UART_Receive_IT(&huart1, &hm10_uart_rx_byte, 1U);

        if (byte == '\r')
        {
            return;
        }

        if (byte == '\n')
        {
            hm10_isr_line[hm10_isr_line_len] = '\0';

            if ((hm10_isr_line_len > 0U) && (hm10_isr_line_overflow == 0U))
            {
                if (strcmp(hm10_isr_line, DEBUG_TERMINAL_PING_REPLY) == 0)
                {
                    if (hm10_bottom_led_semaphore != NULL)
                    {
                        osSemaphoreRelease(hm10_bottom_led_semaphore);
                    }

                    hm10_ping_reply_flag = 1U;
                }
                else
                {
                    if (hm10_top_led_semaphore != NULL)
                    {
                        osSemaphoreRelease(hm10_top_led_semaphore);
                    }

                    /* Hand the raw text to HM10_Thread and let it parse there.
                     * Parsing here would put strtod, and the libc malloc it can
                     * reach, inside an interrupt running on the 1KB main stack. */
                    if ((hm10_isr_line_len < TRAIL_GUI_RENDER_LINE_SIZE) &&
                        (hm10_render_queue != NULL))
                    {
                        TrailGui_RenderWidgetPacket render_packet = {0};

                        render_packet.widget_state = RENDER_WIDGET_STATE_ACTIVE;
                        memcpy(render_packet.line,
                               hm10_isr_line,
                               (size_t)hm10_isr_line_len + 1U);

                        if (osMessageQueuePut(hm10_render_queue, &render_packet, 0U, 0U) != osOK)
                        {
                            hm10_render_queue_drops++;
                        }
                    }
                }
            }

            hm10_isr_line_len = 0U;
            hm10_isr_line[0] = '\0';
            hm10_isr_line_overflow = 0U;
            return;
        }

        if (hm10_isr_line_len < (HM10_DEFAULT_LINE_SIZE - 1U))
        {
            hm10_isr_line[hm10_isr_line_len++] = (char)byte;
        }
        else
        {
            /* Keep swallowing bytes until the next newline resynchronises the
             * stream, so the tail of an over-long line is never mistaken for
             * the start of a fresh packet. */
            hm10_isr_line_overflow = 1U;
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

        TrailGui_RenderWidgetPacket render_packet = {0};

        render_packet.widget_state = (hm10_connection_state == GPIO_PIN_SET)
                                          ? RENDER_WIDGET_STATE_CONNECTED
                                          : RENDER_WIDGET_STATE_IDLE;

        if (osMessageQueuePut(hm10_render_queue, &render_packet, 0U, 0U) != osOK)
        {
            hm10_render_queue_drops++;
        }
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
