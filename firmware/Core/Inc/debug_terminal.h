#ifndef DEBUG_TERMINAL_H
#define DEBUG_TERMINAL_H

#ifdef __cplusplus
extern "C" {

#endif

#include "stm32h7xx_hal.h"
#include "hm10_packet.h"
#include "mpu6050_packet.h"
#include <stdint.h>

#define DEBUG_TERMINAL_PING_COUNT 4U
#define DEBUG_TERMINAL_PING_TIMEOUT_MS 1000U
#define DEBUG_TERMINAL_PING_PACKET "trailhud:ping"
#define DEBUG_TERMINAL_PING_REPLY "trailhud:pong"

/**
 * @brief Lists the debug terminal display modes controlled from PuTTY input.
 *
 * Fields:
 * - DEBUG_TERMINAL_MODE_WAITING: Terminal waits without periodic debug output.
 * - DEBUG_TERMINAL_MODE_PINGS: Terminal displays ping-related BLE debug output.
 * - DEBUG_TERMINAL_MODE_HM10_DATA: Terminal displays parsed phone data packets
 *   received over BLE.
 * - DEBUG_TERMINAL_MODE_MPU6050_DATA: Terminal displays formatted MPU-6050
 *   accelerometer and gyroscope packets.
 */
typedef enum
{
    DEBUG_TERMINAL_MODE_WAITING = 0,
    DEBUG_TERMINAL_MODE_HELP,
    DEBUG_TERMINAL_MODE_PINGS,
    DEBUG_TERMINAL_MODE_HM10_DATA,
    DEBUG_TERMINAL_MODE_MPU6050_DATA
} DebugTerminalMode;

/**
 * @brief Creates the mutex that serialises debug terminal output.
 * @return Nothing.
 *
 * Must be called after osKernelInitialize() and before any thread that prints
 * is started. Printing before this runs is still safe: the terminal is
 * single-threaded during start-up and the lock is skipped until it exists.
 */
void DebugTerminal_Init(void);

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
void DebugTerminal_FormatFixed(char* out, size_t out_size, double value, uint8_t decimals, uint8_t width);

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
 * @return Pointer to a static string: "WAITING", "PINGS", "PHONE DATA",
 *         "GYROSCOPE DATA", or "UNKNOWN" for values outside DebugTerminalMode.
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
                                    const HM10_DataPacket* packet);

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
                                      const MPU6050_DataPacket* packet);

/**
 * @brief Handles pending keyboard input from the debug terminal UART.
 * @param huart STM32 HAL UART handle connected to the debug terminal; NULL is
 *              allowed and causes no input handling.
 * @param mode Pointer to the current debug terminal mode; NULL is allowed and
 *             causes no input handling.
 * @return Nothing.
 */
void DebugTerminal_HandleInput(UART_HandleTypeDef* huart, volatile DebugTerminalMode* mode);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_TERMINAL_H */
