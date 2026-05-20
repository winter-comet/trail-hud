#include "debug_terminal.h"

#include <stdio.h>
#include <string.h>

#define DEBUG_TERMINAL_PRINT_SIZE 220U

static char debug_print[DEBUG_TERMINAL_PRINT_SIZE];

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
        "| Board   : STM32H750B-DK                                   |\r\n"
        "| BLE     : HM-10 / AT-09 on USART1                         |\r\n"
        "| Debug   : USART3 / ST-LINK VCP / PuTTY / 9600 8N1          |\r\n"
        "+-----------------------------------------------------------+\r\n"
        "| Default mode : PINGS                                      |\r\n"
        "| Press 'm'   : switch between PINGS and PHONE DATA          |\r\n"
        "+-----------------------------------------------------------+\r\n"
        "| PINGS      : show STM32 -> HM-10 ping activity             |\r\n"
        "| PHONE DATA : show complete lines received from the phone   |\r\n"
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
    return (mode == DEBUG_TERMINAL_MODE_PHONE_DATA) ? "PHONE DATA" : "PINGS";
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
    int len;
    uint16_t tx_len;
    size_t packet_len;

    if ((huart == NULL) || (packet == NULL))
    {
        return;
    }

    packet_len = strlen(packet);
    if (packet_len == 0U)
    {
        return;
    }

    if ((packet[0] == '[') && (packet[packet_len - 1U] == ']'))
    {
        DebugTerminal_PrintLine(huart, "BLE <- PHONE: phone pose packet received");
    }
    else
    {
        DebugTerminal_PrintLine(huart, "BLE <- PHONE: unrecognized line received");
    }

    len = snprintf(debug_print,
                   sizeof(debug_print),
                   " %s\r\n",
                   packet);

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
            *mode = (*mode == DEBUG_TERMINAL_MODE_PINGS)
                        ? DEBUG_TERMINAL_MODE_PHONE_DATA
                        : DEBUG_TERMINAL_MODE_PINGS;

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
            DebugTerminal_PrintLine(debug_uart,
                                    "BLE <- PHONE: RX line overflow, dropped partial packet");
        }
    }
}
