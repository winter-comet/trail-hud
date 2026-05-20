#ifndef DEBUG_TERMINAL_H
#define DEBUG_TERMINAL_H

#ifdef __cplusplus
extern "C" {

#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

typedef enum
{
    DEBUG_TERMINAL_MODE_PINGS = 0,
    DEBUG_TERMINAL_MODE_PHONE_DATA
} DebugTerminalMode;

void DebugTerminal_PrintTitle(UART_HandleTypeDef* huart);
const char* DebugTerminal_ModeName(DebugTerminalMode mode);
void DebugTerminal_PrintLine(UART_HandleTypeDef* huart, const char* text);
void DebugTerminal_PrintMode(UART_HandleTypeDef* huart, DebugTerminalMode mode);
void DebugTerminal_PrintBlePacket(UART_HandleTypeDef* huart, const char* packet);

void DebugTerminal_HandleInput(UART_HandleTypeDef* huart,
                               DebugTerminalMode* mode);

void DebugTerminal_HandleBleRxByte(UART_HandleTypeDef* debug_uart,
                                   uint8_t rx_byte,
                                   char* rx_line,
                                   uint16_t* rx_len,
                                   uint16_t rx_line_size,
                                   DebugTerminalMode mode);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_TERMINAL_H */
