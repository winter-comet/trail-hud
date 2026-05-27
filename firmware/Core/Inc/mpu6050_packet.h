#ifndef MPU6050_PACKET_H
#define MPU6050_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MPU6050_PACKET_RAW_DATA_LENGTH 14U

/**
 * @brief Stores one accelerometer, gyroscope, and temperature sample from the MPU-6050.
 *
 * Fields:
 * - accel_x_raw: Raw signed 16-bit X-axis accelerometer register value.
 * - accel_y_raw: Raw signed 16-bit Y-axis accelerometer register value.
 * - accel_z_raw: Raw signed 16-bit Z-axis accelerometer register value.
 * - temperature_raw: Raw signed 16-bit temperature register value.
 * - gyro_x_raw: Raw signed 16-bit X-axis gyroscope register value.
 * - gyro_y_raw: Raw signed 16-bit Y-axis gyroscope register value.
 * - gyro_z_raw: Raw signed 16-bit Z-axis gyroscope register value.
 * - accel_x_g: X-axis acceleration in g, assuming the default +/-2 g range.
 * - accel_y_g: Y-axis acceleration in g, assuming the default +/-2 g range.
 * - accel_z_g: Z-axis acceleration in g, assuming the default +/-2 g range.
 * - temperature_c: Internal sensor temperature in degrees Celsius.
 * - gyro_x_dps: X-axis angular velocity in degrees per second, assuming the
 *   default +/-250 dps range.
 * - gyro_y_dps: Y-axis angular velocity in degrees per second, assuming the
 *   default +/-250 dps range.
 * - gyro_z_dps: Z-axis angular velocity in degrees per second, assuming the
 *   default +/-250 dps range.
 */
typedef struct
{
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temperature_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float temperature_c;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
} MPU6050_DataPacket;

/**
 * @brief Fills one MPU-6050 data packet from a raw 14-byte sensor register dump.
 * @param packet Output packet receiving raw and scaled sensor values; NULL is not allowed.
 * @param raw_data Raw bytes read from ACCEL_XOUT_H through GYRO_ZOUT_L; NULL is
 *                 not allowed and must contain MPU6050_PACKET_RAW_DATA_LENGTH bytes.
 * @return 1 when the packet is filled, or 0 when packet or raw_data is NULL.
 */
uint8_t MPU6050_FillDataPacketFromRaw(MPU6050_DataPacket* packet, const uint8_t* raw_data);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_PACKET_H */
