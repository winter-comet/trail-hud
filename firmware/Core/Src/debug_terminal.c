#include "debug_terminal.h"

#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>

#define DEBUG_TERMINAL_PRINT_SIZE 220U

/* One shared formatting buffer for every printer in this file, so MainThread
 * and HM10_Thread must not format into it at the same time. The mutex covers
 * the transmit as well as the formatting, which also stops two complete lines
 * from interleaving halfway through on the wire. */
static char debug_print[DEBUG_TERMINAL_PRINT_SIZE];
static osMutexId_t debug_terminal_mutex;

/**
 * @brief Creates the mutex that serialises debug terminal output.
 * @return Nothing.
 */
void DebugTerminal_Init(void)
{
    static const osMutexAttr_t mutex_attr = {
        .name = "DebugTerminal",
        .attr_bits = osMutexPrioInherit,
    };

    debug_terminal_mutex = osMutexNew(&mutex_attr);
}

/**
 * @brief Takes the debug terminal lock when one is available.
 * @return Nothing.
 *
 * The start-up banner and the boot stage lines are printed before the
 * scheduler runs, where acquiring would block forever. Nothing else is
 * printing at that point, so skipping the lock is safe.
 */
static void DebugTerminal_Lock(void)
{
    if ((debug_terminal_mutex != NULL) && (osKernelGetState() == osKernelRunning))
    {
        (void)osMutexAcquire(debug_terminal_mutex, osWaitForever);
    }
}

/**
 * @brief Releases the debug terminal lock when one is held.
 * @return Nothing.
 */
static void DebugTerminal_Unlock(void)
{
    if ((debug_terminal_mutex != NULL) && (osKernelGetState() == osKernelRunning))
    {
        (void)osMutexRelease(debug_terminal_mutex);
    }
}

typedef struct
{
    const char mode_trigger;
    const char* mode_display_name;
    DebugTerminalMode mode;
} Command;

static const Command commands[] = {
    {'h', "HELP", DEBUG_TERMINAL_MODE_HELP},
    {'H', "HELP", DEBUG_TERMINAL_MODE_HELP},
    {'w', "WAITING", DEBUG_TERMINAL_MODE_WAITING},
    {'W', "WAITING", DEBUG_TERMINAL_MODE_WAITING},
    {'p', "PINGS", DEBUG_TERMINAL_MODE_PINGS},
    {'P', "PINGS", DEBUG_TERMINAL_MODE_PINGS},
    {'d', "PHONE DATA", DEBUG_TERMINAL_MODE_HM10_DATA},
    {'D', "PHONE DATA", DEBUG_TERMINAL_MODE_HM10_DATA},
    {'i', "GYROSCOPE DATA", DEBUG_TERMINAL_MODE_MPU6050_DATA},
    {'I', "GYROSCOPE DATA", DEBUG_TERMINAL_MODE_MPU6050_DATA},
    {'\0', NULL, DEBUG_TERMINAL_MODE_WAITING},
};

/**
 * @brief Calculates an unsigned integer power of ten.
 * @param exponent Decimal exponent to apply; expected to be small enough that
 *                 10^exponent fits in uint32_t.
 * @return 10 raised to exponent as a uint32_t value.
 */
static uint32_t DebugTerminal_Pow10(uint8_t exponent)
{
    uint32_t result = 1U;

    while (exponent > 0U)
    {
        result *= 10U;
        exponent--;
    }

    return result;
}

/**
 * @brief Formats a floating-point value as fixed-point text with optional left padding.
 * @param out Destination character buffer; NULL is allowed and causes no output.
 * @param out_size Size of out in bytes, including the null terminator; 0 is
 *                 allowed and causes no output.
 * @param value Floating-point value to format.
 * @param decimals Number of digits to print after the decimal point.
 * @param width Minimum output width in characters; shorter values are padded
 *              with leading spaces.
 * @return Nothing.
 */
void DebugTerminal_FormatFixed(char* out,
                                      size_t out_size,
                                      double value,
                                      uint8_t decimals,
                                      uint8_t width)
{
    char raw[40];
    const char* sign = "";
    double abs_value;
    uint32_t scale;
    uint64_t scaled_value;
    uint32_t whole;
    uint32_t frac;
    int raw_len;
    size_t raw_size;
    size_t pad_count;
    size_t out_index = 0U;
    size_t raw_index = 0U;

    if ((out == NULL) || (out_size == 0U))
    {
        return;
    }

    out[0] = '\0';

    if (value < 0.0)
    {
        sign = "-";
        abs_value = -value;
    }
    else
    {
        abs_value = value;
    }

    scale = DebugTerminal_Pow10(decimals);
    scaled_value = (uint64_t)((abs_value * (double)scale) + 0.5);
    whole = (uint32_t)(scaled_value / scale);
    frac = (uint32_t)(scaled_value % scale);

    if (scaled_value == 0U)
    {
        /* The magnitude rounded away entirely at this precision, so a sign
         * would describe something the reader cannot see: "-0.00" reads as a
         * negative measurement when every printed digit is zero. */
        sign = "";
    }

    raw_len = snprintf(raw,
                       sizeof(raw),
                       "%s%lu.%0*lu",
                       sign,
                       (unsigned long)whole,
                       (int)decimals,
                       (unsigned long)frac);

    if (raw_len <= 0)
    {
        return;
    }

    raw_size = strlen(raw);
    pad_count = (raw_size < width) ? ((size_t)width - raw_size) : 0U;

    while ((pad_count > 0U) && (out_index < (out_size - 1U)))
    {
        out[out_index++] = ' ';
        pad_count--;
    }

    while ((raw[raw_index] != '\0') && (out_index < (out_size - 1U)))
    {
        out[out_index++] = raw[raw_index++];
    }

    out[out_index] = '\0';
}

/**
 * @brief Converts a snprintf-style length into a safe UART transmit length.
 * @param len Length returned by snprintf; values less than or equal to 0 produce
 *            a transmit length of 0.
 * @param buffer_size Size of the source print buffer in bytes; must be greater
 *                    than 0.
 * @return 0 when len is not positive, buffer_size - 1 when len would exceed or
 *         fill the buffer, otherwise len cast to uint16_t.
 */
static uint16_t DebugTerminal_ClampLength(int len, size_t buffer_size)
{
    if (len <= 0)
    {
        return 0U;
    }

    if (len >= (int)buffer_size)
    {
        return (uint16_t)(buffer_size - 1U);
    }

    return (uint16_t)len;
}

/**
 * @brief Transmits the line already formatted into debug_print, then unlocks.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is not
 *              allowed, callers check it before taking the lock.
 * @param len Length returned by the snprintf that filled debug_print.
 * @return Nothing.
 */
static void DebugTerminal_TransmitAndUnlock(UART_HandleTypeDef* huart, int len)
{
    uint16_t tx_len = DebugTerminal_ClampLength(len, sizeof(debug_print));

    if (tx_len != 0U)
    {
        HAL_UART_Transmit(huart, (uint8_t*)debug_print, tx_len, 1000U);
    }

    DebugTerminal_Unlock();
}

/**
 * @brief Prints one prefixed line to the debug terminal.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param text Null-terminated text to print after the "> " prefix; NULL is
 *             allowed and is printed as "(null)".
 * @return Nothing.
 */
void DebugTerminal_PrintLine(UART_HandleTypeDef* huart, const char* text)
{
    int len;

    if (huart == NULL)
    {
        return;
    }

    DebugTerminal_Lock();

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> %s\r\n",
                   (text != NULL) ? text : "(null)");

    DebugTerminal_TransmitAndUnlock(huart, len);
}

/**
 * @brief Prints the startup banner and command overview for the debug terminal.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @return Nothing.
 */
void DebugTerminal_PrintTitle(UART_HandleTypeDef* huart)
{
    static const char boot[] =
        "\r\n"
        "+==========================================================+\r\n"
        "| TRAIL-HUD STM32 DEBUG TERMINAL                           |\r\n"
        "+==========================================================+\r\n"
        "| Board : STM32H750B-DK                                    |\r\n"
        "| BLE   : HM-10 / AT-09 on USART1                          |\r\n"
        "| IMU   : MPU-6050 on I2C4                                 |\r\n"
        "| Debug : USART3 / ST-LINK VCP / PuTTY / 9600 8N1          |\r\n"
        "+----------------------------------------------------------+\r\n"
        "| Default mode : WAITING                                   |\r\n"
        "| Press 'w'    : WAITING                                   |\r\n"
        "| Press 'p'    : PINGS                                     |\r\n"
        "| Press 'd'    : PHONE DATA                                |\r\n"
        "| Press 'i'    : GYROSCOPE DATA                            |\r\n"
        "| Press 'h'    : HELP                                      |\r\n"
        "+----------------------------------------------------------+\r\n"
        "| WAITING        : no periodic debug output                |\r\n"
        "| PINGS          : send 4 BLE pings and wait for replies   |\r\n"
        "| PHONE DATA     : show formatted phone packets            |\r\n"
        "| GYROSCOPE DATA : show accelerometer and gyroscope packets|\r\n"
        "| HELP           : print command table, keep current mode  |\r\n"
        "+==========================================================+\r\n"
        "\r\n";

    if (huart == NULL)
    {
        return;
    }

    DebugTerminal_Lock();
    HAL_UART_Transmit(huart, (uint8_t*)boot, (uint16_t)(sizeof(boot) - 1U), 2000U);
    DebugTerminal_Unlock();
}

/**
 * @brief Checks whether a received debug-terminal byte requests help.
 * @param rx_byte One byte received from the debug terminal UART.
 * @return 1 when rx_byte is a help command, otherwise 0.
 */
static uint8_t DebugTerminal_IsHelpCommand(uint8_t rx_byte)
{
    for (int command_ix = 0; commands[command_ix].mode_trigger != '\0'; command_ix++)
    {
        if ((commands[command_ix].mode == DEBUG_TERMINAL_MODE_HELP) &&
            (rx_byte == (uint8_t)commands[command_ix].mode_trigger))
        {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief Prints only the debug-terminal command table.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @return Nothing.
 */
static void DebugTerminal_PrintHelpTable(UART_HandleTypeDef* huart)
{
    static const char command_table[] =
        "\r\n"
        "+==========================================================+\r\n"
        "| DEBUG TERMINAL COMMANDS                                  |\r\n"
        "+==========================================================+\r\n"
        "| Press 'w' or 'W' : WAITING                               |\r\n"
        "| Press 'p' or 'P' : PINGS                                 |\r\n"
        "| Press 'd' or 'D' : PHONE DATA                            |\r\n"
        "| Press 'i' or 'I' : GYROSCOPE DATA                        |\r\n"
        "| Press 'h' or 'H' : HELP                                  |\r\n"
        "+----------------------------------------------------------+\r\n"
        "| HELP prints command table and keeps current mode         |\r\n"
        "+==========================================================+\r\n"
        "\r\n";

    if (huart == NULL)
    {
        return;
    }

    DebugTerminal_Lock();
    HAL_UART_Transmit(huart,
                      (uint8_t*)command_table,
                      (uint16_t)(sizeof(command_table) - 1U),
                      2000U);
    DebugTerminal_Unlock();
}

/**
 * @brief Prints one parsed phone data packet as a single aligned terminal line.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param packet Parsed phone packet to print; NULL is allowed and causes no
 *               output. Latitude and longitude are printed in degrees,
 *               altitude and horizontal accuracy in meters, and the
 *               orientation quaternion as unitless components. A packet that
 *               carried no horizontal accuracy prints "n/a" for that field.
 * @return Nothing.
 */
void DebugTerminal_PrintPhonePacket(UART_HandleTypeDef* huart,
                                    const HM10_DataPacket* packet)
{
    int len;
    char lat[20];
    char lon[20];
    char alt[16];
    char hacc[16];
    char qw[16];
    char qx[16];
    char qy[16];
    char qz[16];

    if ((huart == NULL) || (packet == NULL))
    {
        return;
    }

    DebugTerminal_FormatFixed(lat, sizeof(lat), packet->lat_deg, 6U, 12U);
    DebugTerminal_FormatFixed(lon, sizeof(lon), packet->lon_deg, 6U, 12U);
    DebugTerminal_FormatFixed(alt, sizeof(alt), packet->alt_m, 2U, 9U);
    DebugTerminal_FormatFixed(qw, sizeof(qw), packet->qw, 5U, 9U);
    DebugTerminal_FormatFixed(qx, sizeof(qx), packet->qx, 5U, 9U);
    DebugTerminal_FormatFixed(qy, sizeof(qy), packet->qy, 5U, 9U);
    DebugTerminal_FormatFixed(qz, sizeof(qz), packet->qz, 5U, 9U);

    if (packet->has_hacc != 0U)
    {
        DebugTerminal_FormatFixed(hacc, sizeof(hacc), packet->hacc_m, 2U, 7U);
    }
    else
    {
        snprintf(hacc, sizeof(hacc), "%7s", "n/a");
    }

    DebugTerminal_Lock();

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> BLE     : lat:%s | lon:%s | alt:%s m | hacc:%s m | qw:%s | qx:%s | qy:%s | qz:%s\r\n",
                   lat,
                   lon,
                   alt,
                   hacc,
                   qw,
                   qx,
                   qy,
                   qz);

    DebugTerminal_TransmitAndUnlock(huart, len);
}

/**
 * @brief Prints one formatted MPU-6050 accelerometer and gyroscope packet.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param packet MPU-6050 data packet to print; NULL is allowed and causes no
 *               output. Accelerometer values are printed in g, gyroscope values
 *               in degrees per second, and temperature in degrees Celsius.
 * @return Nothing.
 */
void DebugTerminal_PrintMpu6050Packet(UART_HandleTypeDef* huart,
                                      const MPU6050_DataPacket* packet)
{
    int len;
    char accel_x[16];
    char accel_y[16];
    char accel_z[16];
    char gyro_x[16];
    char gyro_y[16];
    char gyro_z[16];
    char temp[16];

    if ((huart == NULL) || (packet == NULL))
    {
        return;
    }

    DebugTerminal_FormatFixed(accel_x, sizeof(accel_x), (double)packet->accel_x_g, 3U, 8U);
    DebugTerminal_FormatFixed(accel_y, sizeof(accel_y), (double)packet->accel_y_g, 3U, 8U);
    DebugTerminal_FormatFixed(accel_z, sizeof(accel_z), (double)packet->accel_z_g, 3U, 8U);
    DebugTerminal_FormatFixed(gyro_x, sizeof(gyro_x), (double)packet->gyro_x_dps, 2U, 9U);
    DebugTerminal_FormatFixed(gyro_y, sizeof(gyro_y), (double)packet->gyro_y_dps, 2U, 9U);
    DebugTerminal_FormatFixed(gyro_z, sizeof(gyro_z), (double)packet->gyro_z_dps, 2U, 9U);
    DebugTerminal_FormatFixed(temp, sizeof(temp), (double)packet->temperature_c, 2U, 7U);

    DebugTerminal_Lock();

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> MPU-6050: ax:%s g | ay:%s g | az:%s g | gx:%s dps | gy:%s dps | gz:%s dps | temp:%s C\r\n",
                   accel_x,
                   accel_y,
                   accel_z,
                   gyro_x,
                   gyro_y,
                   gyro_z,
                   temp);

    DebugTerminal_TransmitAndUnlock(huart, len);
}

/**
 * @brief Converts a debug terminal mode value to a readable mode name.
 * @param mode Debug terminal mode to convert.
 * @return Pointer to a static string: "WAITING", "PINGS", "PHONE DATA",
 *         "GYROSCOPE DATA", or "UNKNOWN" for values outside DebugTerminalMode.
 */
const char* DebugTerminal_ModeName(DebugTerminalMode mode)
{
    const char* mode_name = "UNKNOWN";

    for (int command_ix = 0; commands[command_ix].mode_display_name != NULL; command_ix++)
    {
        if (commands[command_ix].mode == mode)
        {
            mode_name = commands[command_ix].mode_display_name;
            break;
        }
    }

    return mode_name;
}

/**
 * @brief Prints the currently selected debug terminal mode.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param mode Debug terminal mode to print.
 * @return Nothing.
 */
void DebugTerminal_PrintMode(UART_HandleTypeDef* huart, DebugTerminalMode mode)
{
    int len;

    if (huart == NULL)
    {
        return;
    }

    DebugTerminal_Lock();

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> DEBUG MODE: %s\r\n",
                   DebugTerminal_ModeName(mode));

    DebugTerminal_TransmitAndUnlock(huart, len);
}

/**
 * @brief Handles pending keyboard input from the debug terminal UART.
 * @param huart STM32 HAL UART handle connected to the debug terminal; NULL is
 *              allowed and causes no input handling.
 * @param mode Pointer to the current debug terminal mode; NULL is allowed and
 *             causes no input handling.
 * @return Nothing.
 */
void DebugTerminal_HandleInput(UART_HandleTypeDef* huart, volatile DebugTerminalMode* mode)
{
    uint8_t rx_byte = 0U;

    if ((huart == NULL) || (mode == NULL))
    {
        return;
    }

    while (HAL_UART_Receive(huart, &rx_byte, 1U, 0U) == HAL_OK)
    {
        if ((rx_byte == '\r') || (rx_byte == '\n'))
        {
            continue;
        }

        if (DebugTerminal_IsHelpCommand(rx_byte) != 0U)
        {
            /* HELP only prints. Both the boot banner and the command table
             * promise the active mode survives it, so pressing 'h' while
             * PHONE DATA is running must not silently cancel it. */
            DebugTerminal_PrintHelpTable(huart);
            DebugTerminal_PrintMode(huart, *mode);
            continue;
        }

        for (int command_ix = 0; commands[command_ix].mode_trigger != '\0'; command_ix++)
        {
            if (rx_byte == (uint8_t)commands[command_ix].mode_trigger)
            {
                *mode = commands[command_ix].mode;
                DebugTerminal_PrintMode(huart, *mode);
                break;
            }
        }
    }
}
