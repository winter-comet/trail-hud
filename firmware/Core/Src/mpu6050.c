#include "mpu6050.h"
#include <stddef.h>

#define MPU6050_REG_SMPLRT_DIV 0x19U
#define MPU6050_REG_CONFIG 0x1AU
#define MPU6050_REG_GYRO_CONFIG 0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_PWR_MGMT_1 0x6BU
#define MPU6050_REG_WHO_AM_I 0x75U

#define MPU6050_WHO_AM_I_VALUE 0x68U
#define MPU6050_DLPF_CFG_5HZ 0x06U

/**
 * @brief Converts an STM32 HAL I2C status value to an MPU-6050 library status.
 * @param status HAL status returned by an I2C operation.
 * @return MPU6050_OK for HAL_OK, MPU6050_TIMEOUT for HAL_TIMEOUT, MPU6050_BUSY
 *         for HAL_BUSY, and MPU6050_ERROR for HAL_ERROR or any unknown HAL status.
 */
static MPU6050_StatusTypeDef MPU6050_FromHALStatus(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return MPU6050_OK;
        case HAL_TIMEOUT:
            return MPU6050_TIMEOUT;
        case HAL_BUSY:
            return MPU6050_BUSY;
        case HAL_ERROR:
        default:
            return MPU6050_ERROR;
    }
}

/**
 * @brief Checks whether an MPU-6050 handle is usable for I2C communication.
 * @param mpu6050 MPU-6050 handle to validate; NULL is not allowed.
 * @return MPU6050_OK when the handle and its I2C pointer are valid,
 *         MPU6050_INVALID_ARGUMENT when mpu6050 is NULL, or MPU6050_NOT_INITIALIZED
 *         when mpu6050->hi2c is NULL.
 */
static MPU6050_StatusTypeDef MPU6050_CheckHandle(const MPU6050_HandleTypeDef* mpu6050)
{
    if (mpu6050 == NULL)
    {
        return MPU6050_INVALID_ARGUMENT;
    }

    if (mpu6050->hi2c == NULL)
    {
        return MPU6050_NOT_INITIALIZED;
    }

    return MPU6050_OK;
}

/**
 * @brief Writes one byte to one MPU-6050 register over I2C.
 * @param mpu6050 Initialized MPU-6050 handle; NULL is not allowed.
 * @param register_address Register address to write.
 * @param value Byte value to store in the register.
 * @return MPU6050_OK when the register is written, MPU6050_INVALID_ARGUMENT for
 *         an invalid handle, MPU6050_NOT_INITIALIZED if the handle has no I2C
 *         binding, MPU6050_TIMEOUT if transmission times out, MPU6050_BUSY if
 *         the I2C bus is busy, or MPU6050_ERROR for a HAL I2C error.
 */
static MPU6050_StatusTypeDef MPU6050_WriteRegister(MPU6050_HandleTypeDef* mpu6050,
                                                   uint8_t register_address,
                                                   uint8_t value)
{
    MPU6050_StatusTypeDef status = MPU6050_CheckHandle(mpu6050);

    if (status != MPU6050_OK)
    {
        return status;
    }

    return MPU6050_FromHALStatus(HAL_I2C_Mem_Write(mpu6050->hi2c,
                                                   mpu6050->device_address,
                                                   register_address,
                                                   I2C_MEMADD_SIZE_8BIT,
                                                   &value,
                                                   1U,
                                                   mpu6050->timeout_ms));
}

/**
 * @brief Reads one byte from one MPU-6050 register over I2C.
 * @param mpu6050 Initialized MPU-6050 handle; NULL is not allowed.
 * @param register_address Register address to read.
 * @param value Output pointer receiving the register value; NULL is not allowed.
 * @return MPU6050_OK when the register is read, MPU6050_INVALID_ARGUMENT for
 *         invalid pointers, MPU6050_NOT_INITIALIZED if the handle has no I2C
 *         binding, MPU6050_TIMEOUT if reception times out, MPU6050_BUSY if the
 *         I2C bus is busy, or MPU6050_ERROR for a HAL I2C error.
 */
static MPU6050_StatusTypeDef MPU6050_ReadRegister(MPU6050_HandleTypeDef* mpu6050,
                                                  uint8_t register_address,
                                                  uint8_t* value)
{
    MPU6050_StatusTypeDef status = MPU6050_CheckHandle(mpu6050);

    if (status != MPU6050_OK)
    {
        return status;
    }

    if (value == NULL)
    {
        return MPU6050_INVALID_ARGUMENT;
    }

    return MPU6050_FromHALStatus(HAL_I2C_Mem_Read(mpu6050->hi2c,
                                                  mpu6050->device_address,
                                                  register_address,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  value,
                                                  1U,
                                                  mpu6050->timeout_ms));
}

/**
 * @brief Reads several consecutive bytes from MPU-6050 registers over I2C.
 * @param mpu6050 Initialized MPU-6050 handle; NULL is not allowed.
 * @param start_register First register address to read.
 * @param data Destination buffer for received bytes; NULL is allowed only when length is 0.
 * @param length Number of bytes to read; 0 is allowed and reads nothing.
 * @return MPU6050_OK when all requested bytes are read or length is 0,
 *         MPU6050_INVALID_ARGUMENT for invalid pointers, MPU6050_NOT_INITIALIZED
 *         if the handle has no I2C binding, MPU6050_TIMEOUT if reception times
 *         out, MPU6050_BUSY if the I2C bus is busy, or MPU6050_ERROR for a HAL I2C error.
 */
static MPU6050_StatusTypeDef MPU6050_ReadRegisters(MPU6050_HandleTypeDef* mpu6050,
                                                   uint8_t start_register,
                                                   uint8_t* data,
                                                   uint16_t length)
{
    MPU6050_StatusTypeDef status = MPU6050_CheckHandle(mpu6050);

    if (status != MPU6050_OK)
    {
        return status;
    }

    if ((data == NULL) && (length > 0U))
    {
        return MPU6050_INVALID_ARGUMENT;
    }

    if (length == 0U)
    {
        return MPU6050_OK;
    }

    return MPU6050_FromHALStatus(HAL_I2C_Mem_Read(mpu6050->hi2c,
                                                  mpu6050->device_address,
                                                  start_register,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  data,
                                                  length,
                                                  mpu6050->timeout_ms));
}

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
                                   uint16_t device_address)
{
    HAL_StatusTypeDef hal_status;
    MPU6050_StatusTypeDef status;
    uint8_t who_am_i = 0U;

    if ((mpu6050 == NULL) || (hi2c == NULL))
    {
        return MPU6050_INVALID_ARGUMENT;
    }

    mpu6050->hi2c = hi2c;
    mpu6050->device_address = device_address;
    mpu6050->timeout_ms = MPU6050_DEFAULT_TIMEOUT_MS;

    hal_status = HAL_I2C_IsDeviceReady(mpu6050->hi2c,
                                       mpu6050->device_address,
                                       2U,
                                       mpu6050->timeout_ms);

    if (hal_status == HAL_ERROR)
    {
        return MPU6050_DEVICE_NOT_FOUND;
    }

    status = MPU6050_FromHALStatus(hal_status);

    if (status != MPU6050_OK)
    {
        return status;
    }

    status = MPU6050_ReadRegister(mpu6050, MPU6050_REG_WHO_AM_I, &who_am_i);

    if (status != MPU6050_OK)
    {
        return status;
    }

    if (who_am_i != MPU6050_WHO_AM_I_VALUE)
    {
        return MPU6050_DEVICE_NOT_FOUND;
    }

    status = MPU6050_WriteRegister(mpu6050, MPU6050_REG_PWR_MGMT_1, 0x00U);

    if (status != MPU6050_OK)
    {
        return status;
    }

    /* Only sets the data-ready and FIFO rate, neither of which this firmware
     * uses; the rate that matters is the polling interval in MainThread. Left
     * written so the register is in a known state rather than whatever a warm
     * reset happened to leave behind. */
    status = MPU6050_WriteRegister(mpu6050, MPU6050_REG_SMPLRT_DIV, 0x07U);

    if (status != MPU6050_OK)
    {
        return status;
    }

    /* The reset default of 0x00 leaves the low-pass filter off and the sensor
     * bandwidth at 260Hz. Sampled a few times a second on a moving vehicle,
     * that feeds engine and road vibration straight into the tilt reading, so
     * the filter is narrowed to 5Hz to sit near the rate the display uses. */
    status = MPU6050_WriteRegister(mpu6050, MPU6050_REG_CONFIG, MPU6050_DLPF_CFG_5HZ);

    if (status != MPU6050_OK)
    {
        return status;
    }

    status = MPU6050_WriteRegister(mpu6050, MPU6050_REG_GYRO_CONFIG, 0x00U);

    if (status != MPU6050_OK)
    {
        return status;
    }

    return MPU6050_WriteRegister(mpu6050, MPU6050_REG_ACCEL_CONFIG, 0x00U);
}

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
                                             MPU6050_DataPacket* packet)
{
    MPU6050_StatusTypeDef status;
    uint8_t data[MPU6050_PACKET_RAW_DATA_LENGTH];

    /* Same constant sizes the buffer and bounds the read, so the two cannot
     * drift apart into a stack overflow. */
    status = MPU6050_ReadRegisters(mpu6050,
                                   MPU6050_REG_ACCEL_XOUT_H,
                                   data,
                                   MPU6050_PACKET_RAW_DATA_LENGTH);

    if (status != MPU6050_OK)
    {
        return status;
    }

    if (MPU6050_FillDataPacketFromRaw(packet, data) == 0U)
    {
        return MPU6050_INVALID_ARGUMENT;
    }

    return MPU6050_OK;
}
