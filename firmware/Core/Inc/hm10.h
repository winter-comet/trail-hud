#ifndef HM10_H
#define HM10_H

#ifdef __cplusplus
extern "C" {

#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define HM10_DEFAULT_TIMEOUT_MS            100U
#define HM10_DEFAULT_INTER_BYTE_TIMEOUT_MS   5U

/**
 * @brief Lists all status values returned by the HM-10 helper library.
 *
 * Fields:
 * - HM10_OK: Operation completed successfully.
 * - HM10_ERROR: Underlying HAL UART operation reported a generic error.
 * - HM10_TIMEOUT: Underlying HAL UART operation timed out before completion.
 * - HM10_BUSY: Underlying HAL UART operation reported that the UART was busy.
 * - HM10_INVALID_ARGUMENT: One or more arguments were invalid, such as a NULL
 *   pointer where NULL is not allowed or a value that cannot fit the command.
 * - HM10_NOT_INITIALIZED: The HM-10 handle exists but is not bound to a UART.
 */
typedef enum
{
    HM10_OK = 0,
    HM10_ERROR,
    HM10_TIMEOUT,
    HM10_BUSY,
    HM10_INVALID_ARGUMENT,
    HM10_NOT_INITIALIZED
} HM10_StatusTypeDef;

/**
 * @brief Stores the UART binding and timeout configuration for one HM-10 module.
 *
 * Fields:
 * - huart: STM32 HAL UART handle used for communication with the HM-10 module.
 * - timeout_ms: Default blocking UART transmit/receive timeout in milliseconds.
 * - inter_byte_timeout_ms: Timeout in milliseconds used between received bytes
 *   when reading variable-length HM-10 UART data.
 */
typedef struct
{
    UART_HandleTypeDef* huart;
    uint32_t timeout_ms;
    uint32_t inter_byte_timeout_ms;
} HM10_HandleTypeDef;

/**
 * @brief Binds an HM-10 helper handle to an already initialized STM32 UART.
 * @param hm10 HM-10 handle to initialize; NULL is not allowed.
 * @param huart STM32 HAL UART handle connected to the HM-10 module; NULL is not
 *              allowed and the UART must already be configured.
 * @return HM10_OK on successful binding, or HM10_INVALID_ARGUMENT if hm10 or
 *         huart is NULL.
 */
HM10_StatusTypeDef HM10_Init(HM10_HandleTypeDef* hm10, UART_HandleTypeDef* huart);

/**
 * @brief Updates the default UART timeout values stored in an HM-10 handle.
 * @param hm10 Initialized HM-10 handle to update; NULL is not allowed.
 * @param timeout_ms Default blocking transmit/receive timeout in milliseconds.
 * @param inter_byte_timeout_ms Timeout in milliseconds used between bytes during
 *                              variable-length reads.
 * @return HM10_OK on success, HM10_INVALID_ARGUMENT if hm10 is NULL, or
 *         HM10_NOT_INITIALIZED if the handle is not bound to a UART.
 */
HM10_StatusTypeDef HM10_SetTimeouts(HM10_HandleTypeDef* hm10,
                                    uint32_t timeout_ms,
                                    uint32_t inter_byte_timeout_ms);

/**
 * @brief Sends raw bytes to the HM-10 over its configured UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param data Pointer to the bytes to transmit; NULL is allowed only when
 *             length is 0.
 * @param length Number of bytes to transmit; 0 is allowed and sends nothing.
 * @return HM10_OK on successful transmission or zero-length input,
 *         HM10_INVALID_ARGUMENT for invalid pointers, HM10_NOT_INITIALIZED if
 *         the handle has no UART, HM10_TIMEOUT if transmission times out,
 *         HM10_BUSY if the UART is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_SendBytes(HM10_HandleTypeDef* hm10,
                                  const uint8_t* data,
                                  uint16_t length);

/**
 * @brief Sends a null-terminated text string to the HM-10 over UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param text Null-terminated string to transmit; NULL is not allowed and the
 *             string length must fit in uint16_t.
 * @return HM10_OK on successful transmission, HM10_INVALID_ARGUMENT if text is
 *         NULL or too long, HM10_NOT_INITIALIZED if the handle has no UART,
 *         HM10_TIMEOUT if transmission times out, HM10_BUSY if the UART is
 *         busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_SendString(HM10_HandleTypeDef* hm10,
                                   const char* text);

/**
 * @brief Receives an exact number of bytes from the HM-10 over UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param buffer Destination buffer for received bytes; NULL is allowed only
 *               when length is 0.
 * @param length Number of bytes to receive; 0 is allowed and receives nothing.
 * @return HM10_OK when all requested bytes are received or length is 0,
 *         HM10_INVALID_ARGUMENT for invalid pointers, HM10_NOT_INITIALIZED if
 *         the handle has no UART, HM10_TIMEOUT if reception times out,
 *         HM10_BUSY if the UART is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_ReceiveBytes(HM10_HandleTypeDef* hm10,
                                     uint8_t* buffer,
                                     uint16_t length);

/**
 * @brief Reads a variable-length packet currently available from the HM-10 UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param buffer Destination buffer for received bytes; NULL is not allowed.
 * @param buffer_length Maximum number of bytes to store in buffer; must be
 *                      greater than 0.
 * @param received_length Output pointer that receives the number of bytes read;
 *                        NULL is not allowed and is set to 0 before reading.
 * @param first_byte_timeout_ms Timeout in milliseconds while waiting for the
 *                              first byte.
 * @param inter_byte_timeout_ms Timeout in milliseconds while waiting for each
 *                              following byte after at least one byte arrives.
 * @return HM10_OK when one or more bytes are read or the buffer becomes full,
 *         HM10_TIMEOUT when no first byte arrives in time,
 *         HM10_INVALID_ARGUMENT for invalid pointers or zero buffer length,
 *         HM10_NOT_INITIALIZED if the handle has no UART, HM10_BUSY if the UART
 *         is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_ReadAvailable(HM10_HandleTypeDef* hm10,
                                      uint8_t* buffer,
                                      uint16_t buffer_length,
                                      uint16_t* received_length,
                                      uint32_t first_byte_timeout_ms,
                                      uint32_t inter_byte_timeout_ms);

/**
 * @brief Converts an HM-10 status value to a readable constant-name string.
 * @param status HM-10 status value to convert.
 * @return Pointer to a static string describing the status; returns
 *         "HM10_UNKNOWN_STATUS" for values outside HM10_StatusTypeDef.
 */
const char* HM10_StatusToString(HM10_StatusTypeDef status);

/**
 * @brief Reads one byte from the HM-10 UART using a caller-provided timeout.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param byte Output pointer that receives the byte; NULL is not allowed.
 * @param timeout_ms Blocking receive timeout in milliseconds.
 * @return HM10_OK when one byte is received, HM10_INVALID_ARGUMENT for invalid
 *         pointers, HM10_NOT_INITIALIZED if the handle has no UART,
 *         HM10_TIMEOUT if no byte arrives in time, HM10_BUSY if the UART is
 *         busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_ReadByte(HM10_HandleTypeDef *hm10,
                                 uint8_t *byte,
                                 uint32_t timeout_ms);

/**
 * @brief Sends the HM-10 reset AT command over UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @return HM10_OK when the command is transmitted, HM10_INVALID_ARGUMENT for an
 *         invalid handle, HM10_NOT_INITIALIZED if the handle has no UART,
 *         HM10_TIMEOUT if transmission times out, HM10_BUSY if the UART is
 *         busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_Reset(HM10_HandleTypeDef *hm10);

/**
 * @brief Sends the HM-10 AT command that changes the advertised device name.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param name New advertised name string; NULL and empty strings are not
 *             allowed, and the formatted AT command must fit in the internal
 *             command buffer.
 * @return HM10_OK when the command is transmitted, HM10_INVALID_ARGUMENT if the
 *         name is NULL, empty, or too long, HM10_NOT_INITIALIZED if the handle
 *         has no UART, HM10_TIMEOUT if transmission times out, HM10_BUSY if the
 *         UART is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_SetName(HM10_HandleTypeDef *hm10,
                                const char *name);

/**
 * @brief Sets the HM-10 advertised name, sends a reset command, and waits for reset settling.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param name New advertised name string; NULL and empty strings are not
 *             allowed, and the formatted AT command must fit in the internal
 *             command buffer.
 * @param reset_settle_delay_ms Delay in milliseconds after sending AT+RESET
 *                              before returning.
 * @return HM10_OK when the name command and reset command are transmitted,
 *         HM10_INVALID_ARGUMENT for invalid arguments, HM10_NOT_INITIALIZED if
 *         the handle has no UART, HM10_TIMEOUT if transmission times out,
 *         HM10_BUSY if the UART is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_SetNameAndReset(HM10_HandleTypeDef *hm10,
                                        const char *name,
                                        uint32_t reset_settle_delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* HM10_H */
