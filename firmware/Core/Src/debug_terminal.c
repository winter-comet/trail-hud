#include "debug_terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_TERMINAL_PRINT_SIZE 220U

static char debug_print[DEBUG_TERMINAL_PRINT_SIZE];

typedef struct
{
    const char* mode_trigger;
    const char* mode_name_char;
    DebugTerminalMode mode_name_enum;
} Command;

static const Command commands[] = {
    {"h", "HELP", DEBUG_TERMINAL_MODE_HELP},
    {"H", "HELP", DEBUG_TERMINAL_MODE_HELP},
    {"w", "WAITING", DEBUG_TERMINAL_MODE_WAITING},
    {"W", "WAITING", DEBUG_TERMINAL_MODE_WAITING},
    {"p", "PINGS", DEBUG_TERMINAL_MODE_PINGS},
    {"P", "PINGS", DEBUG_TERMINAL_MODE_PINGS},
    {"d", "PHONE DATA", DEBUG_TERMINAL_MODE_PHONE_DATA},
    {"D", "PHONE DATA", DEBUG_TERMINAL_MODE_PHONE_DATA},
    {"i", "GYROSCOPE DATA", DEBUG_TERMINAL_MODE_MPU6050_DATA},
    {"I", "GYROSCOPE DATA", DEBUG_TERMINAL_MODE_MPU6050_DATA},
    {NULL, NULL, DEBUG_TERMINAL_MODE_WAITING},
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
static void DebugTerminal_FormatFixed(char* out,
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
    uint16_t tx_len;

    if (huart == NULL)
    {
        return;
    }

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> %s\r\n",
                   (text != NULL) ? text : "(null)");

    tx_len = DebugTerminal_ClampLength(len, sizeof(debug_print));

    if (tx_len == 0U)
    {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t*)debug_print, tx_len, 1000U);
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
        "| Press 'i'    : MPU-6050 DATA                             |\r\n"
        "| Press 'h'    : HELP                                      |\r\n"
        "+----------------------------------------------------------+\r\n"
        "| WAITING       : no periodic debug output                 |\r\n"
        "| PINGS         : send 4 BLE pings and wait for replies    |\r\n"
        "| PHONE DATA    : show formatted phone packets             |\r\n"
        "| MPU-6050 DATA : show accelerometer and gyroscope packets |\r\n"
        "| HELP          : print command table, keep current mode   |\r\n"
        "+==========================================================+\r\n"
        "\r\n";

    if (huart == NULL)
    {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t*)boot, (uint16_t)(sizeof(boot) - 1U), 2000U);
}

/**
 * @brief Checks whether a received debug-terminal byte requests help.
 * @param rx_byte One byte received from the debug terminal UART.
 * @return 1 when rx_byte is a help command, otherwise 0.
 */
static uint8_t DebugTerminal_IsHelpCommand(uint8_t rx_byte)
{
    for (int command_ix = 0; commands[command_ix].mode_trigger != NULL; command_ix++)
    {
        if ((commands[command_ix].mode_name_enum == DEBUG_TERMINAL_MODE_HELP) &&
            (rx_byte == (uint8_t)commands[command_ix].mode_trigger[0]))
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
        "| Press 'i' or 'I' : MPU-6050 DATA                         |\r\n"
        "| Press 'h' or 'H' : HELP                                  |\r\n"
        "+----------------------------------------------------------+\r\n"
        "| HELP prints command table and keeps current mode         |\r\n"
        "+==========================================================+\r\n"
        "\r\n";

    if (huart == NULL)
    {
        return;
    }

    HAL_UART_Transmit(huart,
                      (uint8_t*)command_table,
                      (uint16_t)(sizeof(command_table) - 1U),
                      2000U);
}

/**
 * @brief Prints a parsed phone data packet as one aligned debug-terminal line.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param packet Parsed phone packet to print; NULL is allowed and causes no
 *               output.
 * @return Nothing.
 */
static void DebugTerminal_PrintFormattedPhonePacket(UART_HandleTypeDef* huart,
                                                    const HM10_DataPacket* packet)
{
    int len;
    uint16_t tx_len;
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

    tx_len = DebugTerminal_ClampLength(len, sizeof(debug_print));

    if (tx_len == 0U)
    {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t*)debug_print, tx_len, 1000U);
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
    uint16_t tx_len;
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

    tx_len = DebugTerminal_ClampLength(len, sizeof(debug_print));

    if (tx_len == 0U)
    {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t*)debug_print, tx_len, 1000U);
}

/**
 * @brief Parses and prints one BLE phone packet when it matches the expected format.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param packet Null-terminated BLE packet string; NULL, empty, and
 *               unrecognized packets are ignored.
 * @return Nothing.
 */
void DebugTerminal_ParsePhonePacket(UART_HandleTypeDef* huart, const char* packet)
{
    size_t packet_len;
    HM10_DataPacket parsed_packet;

    if ((huart == NULL) || (packet == NULL))
    {
        return;
    }

    packet_len = strlen(packet);

    if (packet_len == 0U)
    {
        return;
    }

    if (HM10_ParseDataPacket(packet, &parsed_packet) != 0U)
    {
        DebugTerminal_PrintFormattedPhonePacket(huart, &parsed_packet);
        return;
    }
}

/**
 * @brief Accumulates BLE RX bytes into newline-terminated packets for debug printing.
 * @param debug_uart STM32 HAL UART handle for debug output; NULL is allowed and
 *                   causes no processing.
 * @param rx_byte One byte received from the HM-10 UART bridge.
 * @param rx_line Destination line buffer used to accumulate bytes until \n;
 *                NULL is allowed and causes no processing.
 * @param rx_len Pointer to the current number of bytes stored in rx_line; NULL
 *               is allowed and causes no processing.
 * @param rx_line_size Size of rx_line in bytes, including space for the null
 *                     terminator; 0 is allowed and causes no processing.
 * @param mode Current debug terminal mode; phone packets are printed only in
 *             DEBUG_TERMINAL_MODE_PHONE_DATA.
 * @return 1 when the completed line matches DEBUG_TERMINAL_PING_REPLY, or 0
 *         when no completed ping reply is detected.
 */
uint8_t DebugTerminal_HandleBleRxByte(UART_HandleTypeDef* debug_uart,
                                      uint8_t rx_byte,
                                      char* rx_line,
                                      uint16_t* rx_len,
                                      uint16_t rx_line_size,
                                      DebugTerminalMode mode)
{
    uint8_t is_ping_reply = 0U;

    if ((debug_uart == NULL) || (rx_line == NULL) || (rx_len == NULL) || (rx_line_size == 0U))
    {
        return 0U;
    }

    if (rx_byte == '\r')
    {
        return 0U;
    }

    if (rx_byte == '\n')
    {
        rx_line[*rx_len] = '\0';

        if (*rx_len > 0U)
        {
            if (strcmp(rx_line, DEBUG_TERMINAL_PING_REPLY) == 0)
            {
                is_ping_reply = 1U;
            }
            else if (mode == DEBUG_TERMINAL_MODE_PHONE_DATA)
            {
                DebugTerminal_ParsePhonePacket(debug_uart, rx_line);
            }
        }

        *rx_len = 0U;
        rx_line[0] = '\0';

        return is_ping_reply;
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

        if (mode == DEBUG_TERMINAL_MODE_PHONE_DATA)
        {
            DebugTerminal_PrintLine(debug_uart, "BLE: RX line overflow, dropped partial packet");
        }
    }

    return 0U;
}

/**
 * @brief Converts a debug terminal mode value to a readable mode name.
 * @param mode Debug terminal mode to convert.
 * @return Pointer to a static string: "WAITING", "PINGS", "PHONE DATA",
 *         "MPU-6050 DATA", or "UNKNOWN" for values outside DebugTerminalMode.
 */
const char* DebugTerminal_ModeName(DebugTerminalMode mode)
{
    const char* mode_name = "UNKNOWN";

    for (int command_ix = 0; commands[command_ix].mode_name_char != NULL; command_ix++)
    {
        if (commands[command_ix].mode_name_enum == mode)
        {
            mode_name = commands[command_ix].mode_name_char;
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
    uint16_t tx_len;

    if (huart == NULL)
    {
        return;
    }

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   "> DEBUG MODE: %s\r\n",
                   DebugTerminal_ModeName(mode));

    tx_len = DebugTerminal_ClampLength(len, sizeof(debug_print));

    if (tx_len == 0U)
    {
        return;
    }

    HAL_UART_Transmit(huart, (uint8_t*)debug_print, tx_len, 1000U);
}

/**
 * @brief Handles pending keyboard input from the debug terminal UART.
 * @param huart STM32 HAL UART handle connected to the debug terminal; NULL is
 *              allowed and causes no input handling.
 * @param mode Pointer to the current debug terminal mode; NULL is allowed and
 *             causes no input handling.
 * @return Nothing.
 */
void DebugTerminal_HandleInput(UART_HandleTypeDef* huart, DebugTerminalMode* mode)
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
            DebugTerminal_PrintHelpTable(huart);
            *mode = DEBUG_TERMINAL_MODE_WAITING;
            continue;
        }

        for (int command_ix = 0; commands[command_ix].mode_trigger != NULL; command_ix++)
        {
            if (rx_byte == (uint8_t)commands[command_ix].mode_trigger[0])
            {
                *mode = commands[command_ix].mode_name_enum;
                DebugTerminal_PrintMode(huart, *mode);
                break;
            }
        }
    }
}
