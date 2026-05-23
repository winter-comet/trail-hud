#ifndef DEBUG_TERMINAL_H
#define DEBUG_TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define DEBUG_TERMINAL_PING_COUNT      4U
#define DEBUG_TERMINAL_PING_TIMEOUT_MS 1000U
#define DEBUG_TERMINAL_PING_PACKET     "trailhud:ping"
#define DEBUG_TERMINAL_PING_REPLY      "trailhud:pong"

/**
 * @brief Lists the debug terminal display modes controlled from PuTTY input.
 *
 * Fields:
 * - DEBUG_TERMINAL_MODE_WAITING: Terminal waits without periodic debug output.
 * - DEBUG_TERMINAL_MODE_PINGS: Terminal displays ping-related BLE debug output.
 * - DEBUG_TERMINAL_MODE_PHONE_DATA: Terminal displays parsed phone data packets
 *   received over BLE.
 */
typedef enum
{
    DEBUG_TERMINAL_MODE_WAITING = 0,
    DEBUG_TERMINAL_MODE_PINGS,
    DEBUG_TERMINAL_MODE_PHONE_DATA
} DebugTerminalMode;

/**
 * @brief Prints the startup banner and command overview for the debug terminal.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @return Nothing.
 */
void DebugTerminal_PrintTitle(UART_HandleTypeDef* huart);

/**
 * @brief Converts a debug terminal mode value to a readable mode name.
 * @param mode Debug terminal mode to convert.
 * @return Pointer to a static string: "WAITING", "PINGS", "PHONE DATA", or
 *         "UNKNOWN" for values outside DebugTerminalMode.
 */
const char* DebugTerminal_ModeName(DebugTerminalMode mode);

/**
 * @brief Prints one prefixed line to the debug terminal.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param text Null-terminated text to print after the "> " prefix; NULL is
 *             allowed and is printed as "(null)".
 * @return Nothing.
 */
void DebugTerminal_PrintLine(UART_HandleTypeDef* huart, const char* text);

/**
 * @brief Prints the currently selected debug terminal mode.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param mode Debug terminal mode to print.
 * @return Nothing.
 */
void DebugTerminal_PrintMode(UART_HandleTypeDef* huart, DebugTerminalMode mode);

/**
 * @brief Parses and prints one BLE phone packet when it matches the expected format.
 * @param huart STM32 HAL UART handle for the debug terminal; NULL is allowed
 *              and causes no output.
 * @param packet Null-terminated BLE packet string; NULL, empty, and
 *               unrecognized packets are ignored.
 * @return Nothing.
 */
void DebugTerminal_PrintBlePacket(UART_HandleTypeDef* huart, const char* packet);

/**
 * @brief Handles pending keyboard input from the debug terminal UART.
 * @param huart STM32 HAL UART handle connected to the debug terminal; NULL is
 *              allowed and causes no input handling.
 * @param mode Pointer to the current debug terminal mode; NULL is allowed and
 *             causes no input handling.
 * @return Nothing.
 */
void DebugTerminal_HandleInput(UART_HandleTypeDef* huart,
                               DebugTerminalMode* mode);

/**
 * @brief Accumulates BLE RX bytes into newline-terminated packets for debug printing.
 * @param debug_uart STM32 HAL UART handle for the debug terminal output; NULL
 *                   is allowed and causes no processing.
 * @param rx_byte Newly received byte from the BLE UART.
 * @param rx_line Line buffer used to accumulate bytes until a newline; NULL is
 *                allowed and causes no processing.
 * @param rx_len Pointer to the current number of bytes stored in rx_line; NULL
 *               is allowed and causes no processing.
 * @param rx_line_size Size of rx_line in bytes, including space for the null
 *                     terminator; 0 is allowed and causes no processing.
 * @param mode Current debug terminal mode; packets are printed only in
 *             DEBUG_TERMINAL_MODE_PHONE_DATA.
 * @return Nothing.
 */
uint8_t DebugTerminal_HandleBleRxByte(UART_HandleTypeDef* debug_uart,
                                      uint8_t rx_byte,
                                      char* rx_line,
                                      uint16_t* rx_len,
                                      uint16_t rx_line_size,
                                      DebugTerminalMode mode);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_TERMINAL_H */
