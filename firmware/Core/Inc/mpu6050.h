#ifndef MPU6050_H
#define MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu6050_packet.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>

#define MPU6050_I2C_ADDRESS_AD0_LOW 0x68U
#define MPU6050_I2C_ADDRESS_AD0_HIGH 0x69U
#define MPU6050_DEFAULT_I2C_ADDRESS ((uint16_t)(MPU6050_I2C_ADDRESS_AD0_LOW << 1U))
#define MPU6050_DEFAULT_TIMEOUT_MS 100U

/**
 * @brief Lists all status values returned by the MPU-6050 helper library.
 *
 * Fields:
 * - MPU6050_OK: Operation completed successfully.
 * - MPU6050_ERROR: Underlying HAL I2C operation reported a generic error.
 * - MPU6050_TIMEOUT: Underlying HAL I2C operation timed out before completion.
 * - MPU6050_BUSY: Underlying HAL I2C operation reported that the I2C bus was busy.
 * - MPU6050_INVALID_ARGUMENT: One or more arguments were invalid, such as a NULL
 *   pointer where NULL is not allowed.
 * - MPU6050_NOT_INITIALIZED: The MPU-6050 handle exists but is not bound to I2C.
 * - MPU6050_DEVICE_NOT_FOUND: No valid MPU-6050 responded at the configured I2C
 *   address or WHO_AM_I returned an unexpected value.
 */
typedef enum
{
    MPU6050_OK = 0,
    MPU6050_ERROR,
    MPU6050_TIMEOUT,
    MPU6050_BUSY,
    MPU6050_INVALID_ARGUMENT,
    MPU6050_NOT_INITIALIZED,
    MPU6050_DEVICE_NOT_FOUND
} MPU6050_StatusTypeDef;

/**
 * @brief Stores the I2C binding and timeout configuration for one MPU-6050 module.
 *
 * Fields:
 * - hi2c: STM32 HAL I2C handle used for communication with the MPU-6050 module.
 * - device_address: 8-bit, left-shifted STM32 HAL I2C address of the MPU-6050.
 * - timeout_ms: Default blocking I2C memory read/write timeout in milliseconds.
 */
typedef struct
{
    I2C_HandleTypeDef* hi2c;
    uint16_t device_address;
    uint32_t timeout_ms;
} MPU6050_HandleTypeDef;

/**
 * @brief Binds an MPU-6050 helper handle to an initialized STM32 I2C peripheral and wakes the sensor.
 * @param mpu6050 MPU-6050 handle to initialize; NULL is not allowed.
 * @param hi2c STM32 HAL I2C handle connected to the MPU-6050 module; NULL is not
 *             allowed and the I2C peripheral must already be configured.
 * @param device_address 8-bit, left-shifted STM32 HAL I2C address; use
 *                       MPU6050_DEFAULT_I2C_ADDRESS when AD0 is connected to GND.
 * @return MPU6050_OK on successful binding and sensor wake-up, MPU6050_INVALID_ARGUMENT
 *         if mpu6050 or hi2c is NULL, MPU6050_DEVICE_NOT_FOUND if the device does
 *         not respond correctly, MPU6050_TIMEOUT if an I2C operation times out,
 *         MPU6050_BUSY if the I2C bus is busy, or MPU6050_ERROR for a HAL I2C error.
 */
MPU6050_StatusTypeDef MPU6050_Init(MPU6050_HandleTypeDef* mpu6050,
                                   I2C_HandleTypeDef* hi2c,
                                   uint16_t device_address);

/**
 * @brief Updates the default I2C timeout value stored in an MPU-6050 handle.
 * @param mpu6050 Initialized MPU-6050 handle to update; NULL is not allowed.
 * @param timeout_ms Default blocking I2C memory read/write timeout in milliseconds.
 * @return MPU6050_OK on success, MPU6050_INVALID_ARGUMENT if mpu6050 is NULL,
 *         or MPU6050_NOT_INITIALIZED if the handle is not bound to I2C.
 */
MPU6050_StatusTypeDef MPU6050_SetTimeout(MPU6050_HandleTypeDef* mpu6050,
                                         uint32_t timeout_ms);

/**
 * @brief Reads the current accelerometer, temperature, and gyroscope data packet from the MPU-6050.
 * @param mpu6050 Initialized MPU-6050 handle; NULL is not allowed.
 * @param packet Output packet receiving raw and scaled sensor values; NULL is not allowed.
 * @return MPU6050_OK when the packet is filled, MPU6050_INVALID_ARGUMENT for invalid
 *         pointers, MPU6050_NOT_INITIALIZED if the handle has no I2C binding,
 *         MPU6050_TIMEOUT if reception times out, MPU6050_BUSY if the I2C bus
 *         is busy, or MPU6050_ERROR for a HAL I2C error.
 */
MPU6050_StatusTypeDef MPU6050_ReadDataPacket(MPU6050_HandleTypeDef* mpu6050,
                                             MPU6050_DataPacket* packet);

/**
 * @brief Converts an MPU-6050 status value to a readable constant-name string.
 * @param status MPU-6050 status value to convert.
 * @return Pointer to a static string describing the status; returns
 *         "MPU6050_UNKNOWN_STATUS" for values outside MPU6050_StatusTypeDef.
 */
const char* MPU6050_StatusToString(MPU6050_StatusTypeDef status);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
