#include "hm10.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Converts an STM32 HAL UART status value to an HM-10 library status.
 * @param status HAL status returned by a UART operation.
 * @return HM10_OK for HAL_OK, HM10_TIMEOUT for HAL_TIMEOUT, HM10_BUSY for
 *         HAL_BUSY, and HM10_ERROR for HAL_ERROR or any unknown HAL status.
 */
static HM10_StatusTypeDef HM10_FromHALStatus(HAL_StatusTypeDef status)
{
    switch (status)
    {
    case HAL_OK:
        return HM10_OK;
    case HAL_TIMEOUT:
        return HM10_TIMEOUT;
    case HAL_BUSY:
        return HM10_BUSY;
    case HAL_ERROR:
    default:
        return HM10_ERROR;
    }
}

/**
 * @brief Checks whether an HM-10 handle is usable for UART communication.
 * @param hm10 HM-10 handle to validate; NULL is not allowed.
 * @return HM10_OK when the handle and its UART pointer are valid,
 *         HM10_INVALID_ARGUMENT when hm10 is NULL, or HM10_NOT_INITIALIZED
 *         when hm10->huart is NULL.
 */
static HM10_StatusTypeDef HM10_CheckHandle(const HM10_HandleTypeDef* hm10)
{
    if (hm10 == NULL)
    {
        return HM10_INVALID_ARGUMENT;
    }

    if (hm10->huart == NULL)
    {
        return HM10_NOT_INITIALIZED;
    }

    return HM10_OK;
}

/**
 * @brief Binds an HM-10 helper handle to an already initialized STM32 UART.
 * @param hm10 HM-10 handle to initialize; NULL is not allowed.
 * @param huart STM32 HAL UART handle connected to the HM-10 module; NULL is not
 *              allowed and the UART must already be configured.
 * @return HM10_OK on successful binding, or HM10_INVALID_ARGUMENT if hm10 or
 *         huart is NULL.
 */
HM10_StatusTypeDef HM10_Init(HM10_HandleTypeDef* hm10, UART_HandleTypeDef* huart)
{
    if ((hm10 == NULL) || (huart == NULL))
    {
        return HM10_INVALID_ARGUMENT;
    }

    hm10->huart = huart;
    hm10->timeout_ms = HM10_DEFAULT_TIMEOUT_MS;
    hm10->inter_byte_timeout_ms = HM10_DEFAULT_INTER_BYTE_TIMEOUT_MS;

    return HM10_OK;
}

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
                                  uint16_t length)
{
    HM10_StatusTypeDef status = HM10_CheckHandle(hm10);

    if (status != HM10_OK)
    {
        return status;
    }

    if ((data == NULL) && (length > 0U))
    {
        return HM10_INVALID_ARGUMENT;
    }

    if (length == 0U)
    {
        return HM10_OK;
    }

    return HM10_FromHALStatus(HAL_UART_Transmit(hm10->huart,
                                                (uint8_t*)data,
                                                length,
                                                hm10->timeout_ms));
}

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
                                   const char* text)
{
    size_t length;

    if (text == NULL)
    {
        return HM10_INVALID_ARGUMENT;
    }

    length = strlen(text);
    if (length > UINT16_MAX)
    {
        return HM10_INVALID_ARGUMENT;
    }

    return HM10_SendBytes(hm10, (const uint8_t*)text, (uint16_t)length);
}

/**
 * @brief Sends the HM-10 reset AT command over UART.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @return HM10_OK when the command is transmitted, HM10_INVALID_ARGUMENT for an
 *         invalid handle, HM10_NOT_INITIALIZED if the handle has no UART,
 *         HM10_TIMEOUT if transmission times out, HM10_BUSY if the UART is
 *         busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_Reset(HM10_HandleTypeDef* hm10)
{
    return HM10_SendString(hm10, "AT+RESET\r\n");
}

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
HM10_StatusTypeDef HM10_SetName(HM10_HandleTypeDef* hm10,
                                const char* name)
{
    char command[40];
    int command_len;

    if ((name == NULL) || (name[0] == '\0'))
    {
        return HM10_INVALID_ARGUMENT;
    }

    command_len = snprintf(command,
                           sizeof(command),
                           "AT+NAME%s\r\n",
                           name);

    if ((command_len <= 0) || (command_len >= (int)sizeof(command)))
    {
        return HM10_INVALID_ARGUMENT;
    }

    return HM10_SendBytes(hm10,
                          (const uint8_t*)command,
                          (uint16_t)command_len);
}

/**
 * @brief Sets the HM-10 advertised name, sends a reset command, and waits for reset settling.
 * @param hm10 Initialized HM-10 handle; NULL is not allowed.
 * @param name New advertised name string; NULL and empty strings are not
 *             allowed, and the formatted AT command must fit in the internal
 *             command buffer.
 * @param reset_settle_delay_ms Delay in milliseconds applied twice: once after
 *                              the name command so the module can store it,
 *                              and again after AT+RESET before returning.
 * @return HM10_OK when the name command and reset command are transmitted,
 *         HM10_INVALID_ARGUMENT for invalid arguments, HM10_NOT_INITIALIZED if
 *         the handle has no UART, HM10_TIMEOUT if transmission times out,
 *         HM10_BUSY if the UART is busy, or HM10_ERROR for a HAL UART error.
 */
HM10_StatusTypeDef HM10_SetNameAndReset(HM10_HandleTypeDef* hm10,
                                        const char* name,
                                        uint32_t reset_settle_delay_ms)
{
    HM10_StatusTypeDef status;

    status = HM10_SetName(hm10, name);
    if (status != HM10_OK)
    {
        return status;
    }

    HAL_Delay(reset_settle_delay_ms);

    status = HM10_Reset(hm10);
    if (status != HM10_OK)
    {
        return status;
    }

    HAL_Delay(reset_settle_delay_ms);

    return HM10_OK;
}