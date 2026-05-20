#ifndef HM10_H
#define HM10_H

#ifdef __cplusplus
extern "C" {

#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define HM10_DEFAULT_TIMEOUT_MS            100U
#define HM10_DEFAULT_INTER_BYTE_TIMEOUT_MS   5U

typedef enum
{
    HM10_OK = 0,
    HM10_ERROR,
    HM10_TIMEOUT,
    HM10_BUSY,
    HM10_INVALID_ARGUMENT,
    HM10_NOT_INITIALIZED
} HM10_StatusTypeDef;

typedef struct
{
    UART_HandleTypeDef* huart;
    uint32_t timeout_ms;
    uint32_t inter_byte_timeout_ms;
} HM10_HandleTypeDef;

/**
 * @brief Binds an HM-10 helper handle to an already initialized STM32 UART.
 *
 * This function does not configure GPIO pins, baud rate, interrupts, DMA, or
 * the HM-10 itself. Configure the USART/UART peripheral in CubeMX first, then
 * pass the generated UART handle here.
 */
HM10_StatusTypeDef HM10_Init(HM10_HandleTypeDef* hm10, UART_HandleTypeDef* huart);

HM10_StatusTypeDef HM10_SetTimeouts(HM10_HandleTypeDef* hm10,
                                    uint32_t timeout_ms,
                                    uint32_t inter_byte_timeout_ms);

HM10_StatusTypeDef HM10_SendBytes(HM10_HandleTypeDef* hm10,
                                  const uint8_t* data,
                                  uint16_t length);

HM10_StatusTypeDef HM10_SendString(HM10_HandleTypeDef* hm10,
                                   const char* text);

/**
 * @brief Receives exactly length bytes, blocking until all bytes arrive or the
 *        configured timeout expires.
 */
HM10_StatusTypeDef HM10_ReceiveBytes(HM10_HandleTypeDef* hm10,
                                     uint8_t* buffer,
                                     uint16_t length);

/**
 * @brief Receives a variable-length UART packet from the HM-10.
 *
 * The function waits up to first_byte_timeout_ms for the first byte. After at
 * least one byte is received, it keeps reading until the buffer is full or no
 * further byte arrives within inter_byte_timeout_ms. This is useful for simple
 * transparent BLE UART testing from apps such as nRF Connect.
 */
HM10_StatusTypeDef HM10_ReadAvailable(HM10_HandleTypeDef* hm10,
                                      uint8_t* buffer,
                                      uint16_t buffer_length,
                                      uint16_t* received_length,
                                      uint32_t first_byte_timeout_ms,
                                      uint32_t inter_byte_timeout_ms);

const char* HM10_StatusToString(HM10_StatusTypeDef status);

HM10_StatusTypeDef HM10_ReadByte(HM10_HandleTypeDef *hm10,
                                 uint8_t *byte,
                                 uint32_t timeout_ms);

HM10_StatusTypeDef HM10_Reset(HM10_HandleTypeDef *hm10);

HM10_StatusTypeDef HM10_SetName(HM10_HandleTypeDef *hm10,
                                const char *name);

HM10_StatusTypeDef HM10_SetNameAndReset(HM10_HandleTypeDef *hm10,
                                        const char *name,
                                        uint32_t reset_settle_delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* HM10_H */
