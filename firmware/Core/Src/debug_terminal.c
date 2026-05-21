#include "debug_terminal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEBUG_TERMINAL_PRINT_SIZE 220U

static uint16_t DebugTerminal_ClampLength(int len, size_t buffer_size);
const char* DebugTerminal_ModeName(DebugTerminalMode mode);
void DebugTerminal_PrintTitle(UART_HandleTypeDef* huart);
void DebugTerminal_PrintLine(UART_HandleTypeDef* huart, const char* text);
void DebugTerminal_PrintMode(UART_HandleTypeDef* huart, DebugTerminalMode mode);
void DebugTerminal_PrintBlePacket(UART_HandleTypeDef* huart, const char* packet);
void DebugTerminal_HandleInput(UART_HandleTypeDef* huart, DebugTerminalMode* mode);
void DebugTerminal_HandleBleRxByte(UART_HandleTypeDef* debug_uart, uint8_t rx_byte, char* rx_line, uint16_t* rx_len, uint16_t rx_line_size, DebugTerminalMode mode);

static char debug_print[DEBUG_TERMINAL_PRINT_SIZE];

typedef struct
{
    double lat_deg;
    double lon_deg;
    double alt_m;
    double hacc_m;
    double qw;
    double qx;
    double qy;
    double qz;
    uint8_t has_hacc;
} DebugTerminalPhonePacket;

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

static const char* DebugTerminal_SkipSpaces(const char* text)
{
    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    return text;
}

static uint8_t DebugTerminal_ConsumeChar(const char** cursor, char expected)
{
    const char* p;

    if ((cursor == NULL) || (*cursor == NULL))
    {
        return 0U;
    }

    p = DebugTerminal_SkipSpaces(*cursor);

    if (*p != expected)
    {
        return 0U;
    }

    *cursor = DebugTerminal_SkipSpaces(p + 1);
    return 1U;
}

static uint8_t DebugTerminal_ReadDouble(const char** cursor, double* value)
{
    char* end = NULL;
    const char* start;

    if ((cursor == NULL) || (*cursor == NULL) || (value == NULL))
    {
        return 0U;
    }

    start = DebugTerminal_SkipSpaces(*cursor);
    *value = strtod(start, &end);

    if (end == start)
    {
        return 0U;
    }

    *cursor = DebugTerminal_SkipSpaces(end);
    return 1U;
}

static uint8_t DebugTerminal_ParsePhonePacket(const char* packet,
                                              DebugTerminalPhonePacket* out)
{
    const char* cursor;

    if ((packet == NULL) || (out == NULL))
    {
        return 0U;
    }

    memset(out, 0, sizeof(*out));

    cursor = packet;

    if (!DebugTerminal_ConsumeChar(&cursor, '[')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->lat_deg)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ',')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->lon_deg)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ',')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->alt_m)) return 0U;

    if (DebugTerminal_ConsumeChar(&cursor, ','))
    {
        out->has_hacc = 1U;

        if (!DebugTerminal_ReadDouble(&cursor, &out->hacc_m)) return 0U;
        if (!DebugTerminal_ConsumeChar(&cursor, ';')) return 0U;
    }
    else
    {
        out->has_hacc = 0U;

        if (!DebugTerminal_ConsumeChar(&cursor, ';')) return 0U;
    }

    if (!DebugTerminal_ReadDouble(&cursor, &out->qw)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ',')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->qx)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ',')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->qy)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ',')) return 0U;
    if (!DebugTerminal_ReadDouble(&cursor, &out->qz)) return 0U;
    if (!DebugTerminal_ConsumeChar(&cursor, ']')) return 0U;

    return (*DebugTerminal_SkipSpaces(cursor) == '\0') ? 1U : 0U;
}

static void DebugTerminal_PrintFormattedPhonePacket(UART_HandleTypeDef* huart,
                                                    const DebugTerminalPhonePacket* packet)
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

    DebugTerminal_FormatFixed(lat,  sizeof(lat),  packet->lat_deg, 6U, 12U);
    DebugTerminal_FormatFixed(lon,  sizeof(lon),  packet->lon_deg, 6U, 12U);
    DebugTerminal_FormatFixed(alt,  sizeof(alt),  packet->alt_m,   2U, 9U);
    DebugTerminal_FormatFixed(qw,   sizeof(qw),   packet->qw,      5U, 9U);
    DebugTerminal_FormatFixed(qx,   sizeof(qx),   packet->qx,      5U, 9U);
    DebugTerminal_FormatFixed(qy,   sizeof(qy),   packet->qy,      5U, 9U);
    DebugTerminal_FormatFixed(qz,   sizeof(qz),   packet->qz,      5U, 9U);

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
                   "> PHONE | lat:%s | lon:%s | alt:%s m | hacc:%s m | q:%s %s %s %s\r\n",
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

void DebugTerminal_PrintTitle(UART_HandleTypeDef* huart)
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
        "| Default mode : WAITING                                    |\r\n"
        "| Press 'm'    : cycle WAITING -> PINGS -> PHONE DATA       |\r\n"
        "| Press 'w'    : WAITING                                    |\r\n"
        "| Press 'p'    : PINGS                                      |\r\n"
        "| Press 'd'    : PHONE DATA                                 |\r\n"
        "+-----------------------------------------------------------+\r\n"
        "| WAITING    : no periodic debug output                     |\r\n"
        "| PINGS      : show STM32 -> HM-10 ping activity            |\r\n"
        "| PHONE DATA : show formatted phone packets                 |\r\n"
        "+===========================================================+\r\n"
        "\r\n";

    if (huart == NULL)
    {
        return;
    }

    HAL_UART_Transmit(huart,
                      (uint8_t*)boot,
                      (uint16_t)(sizeof(boot) - 1U),
                      2000U);
}

const char* DebugTerminal_ModeName(DebugTerminalMode mode)
{
    switch (mode)
    {
    case DEBUG_TERMINAL_MODE_WAITING:
        return "WAITING";

    case DEBUG_TERMINAL_MODE_PINGS:
        return "PINGS";

    case DEBUG_TERMINAL_MODE_PHONE_DATA:
        return "PHONE DATA";

    default:
        return "UNKNOWN";
    }
}

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

    HAL_UART_Transmit(huart,
                      (uint8_t*)debug_print,
                      tx_len,
                      1000U);
}

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

    HAL_UART_Transmit(huart,
                      (uint8_t*)debug_print,
                      tx_len,
                      1000U);
}

void DebugTerminal_PrintBlePacket(UART_HandleTypeDef* huart, const char* packet)
{
    size_t packet_len;
    DebugTerminalPhonePacket parsed_packet;

    if ((huart == NULL) || (packet == NULL))
    {
        return;
    }

    packet_len = strlen(packet);

    if (packet_len == 0U)
    {
        return;
    }

    if (DebugTerminal_ParsePhonePacket(packet, &parsed_packet) != 0U)
    {
        DebugTerminal_PrintFormattedPhonePacket(huart, &parsed_packet);
        return;
    }
}

void DebugTerminal_HandleInput(UART_HandleTypeDef* huart,
                               DebugTerminalMode* mode)
{
    uint8_t rx_byte = 0U;

    if ((huart == NULL) || (mode == NULL))
    {
        return;
    }

    while (HAL_UART_Receive(huart, &rx_byte, 1U, 0U) == HAL_OK)
    {
        if ((rx_byte == 'm') || (rx_byte == 'M'))
        {
            switch (*mode)
            {
            case DEBUG_TERMINAL_MODE_WAITING:
                *mode = DEBUG_TERMINAL_MODE_PINGS;
                break;

            case DEBUG_TERMINAL_MODE_PINGS:
                *mode = DEBUG_TERMINAL_MODE_PHONE_DATA;
                break;

            case DEBUG_TERMINAL_MODE_PHONE_DATA:
            default:
                *mode = DEBUG_TERMINAL_MODE_WAITING;
                break;
            }

            DebugTerminal_PrintMode(huart, *mode);
        }
        else if ((rx_byte == 'w') || (rx_byte == 'W'))
        {
            *mode = DEBUG_TERMINAL_MODE_WAITING;
            DebugTerminal_PrintMode(huart, *mode);
        }
        else if ((rx_byte == 'p') || (rx_byte == 'P'))
        {
            *mode = DEBUG_TERMINAL_MODE_PINGS;
            DebugTerminal_PrintMode(huart, *mode);
        }
        else if ((rx_byte == 'd') || (rx_byte == 'D'))
        {
            *mode = DEBUG_TERMINAL_MODE_PHONE_DATA;
            DebugTerminal_PrintMode(huart, *mode);
        }
    }
}

void DebugTerminal_HandleBleRxByte(UART_HandleTypeDef* debug_uart,
                                   uint8_t rx_byte,
                                   char* rx_line,
                                   uint16_t* rx_len,
                                   uint16_t rx_line_size,
                                   DebugTerminalMode mode)
{
    if ((debug_uart == NULL) ||
        (rx_line == NULL) ||
        (rx_len == NULL) ||
        (rx_line_size == 0U))
    {
        return;
    }

    if (rx_byte == '\r')
    {
        return;
    }

    if (rx_byte == '\n')
    {
        rx_line[*rx_len] = '\0';

        if ((*rx_len > 0U) && (mode == DEBUG_TERMINAL_MODE_PHONE_DATA))
        {
            DebugTerminal_PrintBlePacket(debug_uart, rx_line);
        }

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

        if (mode == DEBUG_TERMINAL_MODE_PHONE_DATA)
        {
            DebugTerminal_PrintLine(debug_uart, "BLE <- PHONE: RX line overflow, dropped partial packet");
        }
    }
}
