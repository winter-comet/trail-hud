#include "hm10.h"
#include <stdio.h>
#include <string.h>

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

HM10_StatusTypeDef HM10_SetTimeouts(HM10_HandleTypeDef* hm10,
                                    uint32_t timeout_ms,
                                    uint32_t inter_byte_timeout_ms)
{
    HM10_StatusTypeDef status = HM10_CheckHandle(hm10);

    if (status != HM10_OK)
    {
        return status;
    }

    hm10->timeout_ms = timeout_ms;
    hm10->inter_byte_timeout_ms = inter_byte_timeout_ms;

    return HM10_OK;
}

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

HM10_StatusTypeDef HM10_ReceiveBytes(HM10_HandleTypeDef* hm10,
                                     uint8_t* buffer,
                                     uint16_t length)
{
    HM10_StatusTypeDef status = HM10_CheckHandle(hm10);

    if (status != HM10_OK)
    {
        return status;
    }

    if ((buffer == NULL) && (length > 0U))
    {
        return HM10_INVALID_ARGUMENT;
    }

    if (length == 0U)
    {
        return HM10_OK;
    }

    return HM10_FromHALStatus(HAL_UART_Receive(hm10->huart,
                                               buffer,
                                               length,
                                               hm10->timeout_ms));
}

HM10_StatusTypeDef HM10_ReadAvailable(HM10_HandleTypeDef* hm10,
                                      uint8_t* buffer,
                                      uint16_t buffer_length,
                                      uint16_t* received_length,
                                      uint32_t first_byte_timeout_ms,
                                      uint32_t inter_byte_timeout_ms)
{
    HM10_StatusTypeDef status = HM10_CheckHandle(hm10);
    uint16_t count = 0U;
    uint32_t timeout_ms = first_byte_timeout_ms;

    if (received_length != NULL)
    {
        *received_length = 0U;
    }

    if (status != HM10_OK)
    {
        return status;
    }

    if ((buffer == NULL) || (buffer_length == 0U) || (received_length == NULL))
    {
        return HM10_INVALID_ARGUMENT;
    }

    while (count < buffer_length)
    {
        HAL_StatusTypeDef hal_status = HAL_UART_Receive(hm10->huart,
                                                        &buffer[count],
                                                        1U,
                                                        timeout_ms);

        if (hal_status == HAL_OK)
        {
            count++;
            *received_length = count;
            timeout_ms = inter_byte_timeout_ms;
            continue;
        }

        if (hal_status == HAL_TIMEOUT)
        {
            return (count > 0U) ? HM10_OK : HM10_TIMEOUT;
        }

        return HM10_FromHALStatus(hal_status);
    }

    return HM10_OK;
}

const char* HM10_StatusToString(HM10_StatusTypeDef status)
{
    switch (status)
    {
    case HM10_OK:
        return "HM10_OK";
    case HM10_ERROR:
        return "HM10_ERROR";
    case HM10_TIMEOUT:
        return "HM10_TIMEOUT";
    case HM10_BUSY:
        return "HM10_BUSY";
    case HM10_INVALID_ARGUMENT:
        return "HM10_INVALID_ARGUMENT";
    case HM10_NOT_INITIALIZED:
        return "HM10_NOT_INITIALIZED";
    default:
        return "HM10_UNKNOWN_STATUS";
    }
}

HM10_StatusTypeDef HM10_ReadByte(HM10_HandleTypeDef* hm10,
                                 uint8_t* byte,
                                 uint32_t timeout_ms)
{
    HM10_StatusTypeDef status = HM10_CheckHandle(hm10);

    if (status != HM10_OK)
    {
        return status;
    }

    if (byte == NULL)
    {
        return HM10_INVALID_ARGUMENT;
    }

    return HM10_FromHALStatus(HAL_UART_Receive(hm10->huart,
                                               byte,
                                               1U,
                                               timeout_ms));
}

HM10_StatusTypeDef HM10_Reset(HM10_HandleTypeDef* hm10)
{
    return HM10_SendString(hm10, "AT+RESET\r\n");
}

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

    HAL_Delay(500U);

    status = HM10_Reset(hm10);
    if (status != HM10_OK)
    {
        return status;
    }

    HAL_Delay(reset_settle_delay_ms);

    return HM10_OK;
}
