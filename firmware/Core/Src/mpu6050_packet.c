#include "mpu6050_packet.h"

#include <stddef.h>

#define MPU6050_PACKET_ACCEL_SCALE_LSB_PER_G 16384.0f
#define MPU6050_PACKET_GYRO_SCALE_LSB_PER_DPS 131.0f
#define MPU6050_PACKET_TEMP_SCALE_LSB_PER_DEG_C 340.0f
#define MPU6050_PACKET_TEMP_OFFSET_DEG_C 36.53f

/**
 * @brief Combines two MPU-6050 high/low register bytes into one signed 16-bit value.
 * @param high_byte Most significant register byte.
 * @param low_byte Least significant register byte.
 * @return Signed 16-bit value represented by the two input bytes.
 */
static int16_t MPU6050_PacketCombineBytes(uint8_t high_byte, uint8_t low_byte)
{
    return (int16_t)(((uint16_t)high_byte << 8U) | (uint16_t)low_byte);
}

/**
 * @brief Calculates scaled physical values from raw MPU-6050 packet fields.
 * @param packet Packet to update with scaled values; NULL is not allowed.
 * @return Nothing.
 */
static void MPU6050_PacketUpdateScaledValues(MPU6050_DataPacket* packet)
{
    packet->accel_x_g = (float)packet->accel_x_raw / MPU6050_PACKET_ACCEL_SCALE_LSB_PER_G;
    packet->accel_y_g = (float)packet->accel_y_raw / MPU6050_PACKET_ACCEL_SCALE_LSB_PER_G;
    packet->accel_z_g = (float)packet->accel_z_raw / MPU6050_PACKET_ACCEL_SCALE_LSB_PER_G;
    packet->temperature_c = ((float)packet->temperature_raw / MPU6050_PACKET_TEMP_SCALE_LSB_PER_DEG_C) +
                            MPU6050_PACKET_TEMP_OFFSET_DEG_C;
    packet->gyro_x_dps = (float)packet->gyro_x_raw / MPU6050_PACKET_GYRO_SCALE_LSB_PER_DPS;
    packet->gyro_y_dps = (float)packet->gyro_y_raw / MPU6050_PACKET_GYRO_SCALE_LSB_PER_DPS;
    packet->gyro_z_dps = (float)packet->gyro_z_raw / MPU6050_PACKET_GYRO_SCALE_LSB_PER_DPS;
}

/**
 * @brief Fills one MPU-6050 data packet from a raw 14-byte sensor register dump.
 * @param packet Output packet receiving raw and scaled sensor values; NULL is not allowed.
 * @param raw_data Raw bytes read from ACCEL_XOUT_H through GYRO_ZOUT_L; NULL is
 *                 not allowed and must contain MPU6050_PACKET_RAW_DATA_LENGTH bytes.
 * @return 1 when the packet is filled, or 0 when packet or raw_data is NULL.
 */
uint8_t MPU6050_FillDataPacketFromRaw(MPU6050_DataPacket* packet, const uint8_t* raw_data)
{
    if ((packet == NULL) || (raw_data == NULL))
    {
        return 0U;
    }

    packet->accel_x_raw = MPU6050_PacketCombineBytes(raw_data[0], raw_data[1]);
    packet->accel_y_raw = MPU6050_PacketCombineBytes(raw_data[2], raw_data[3]);
    packet->accel_z_raw = MPU6050_PacketCombineBytes(raw_data[4], raw_data[5]);
    packet->temperature_raw = MPU6050_PacketCombineBytes(raw_data[6], raw_data[7]);
    packet->gyro_x_raw = MPU6050_PacketCombineBytes(raw_data[8], raw_data[9]);
    packet->gyro_y_raw = MPU6050_PacketCombineBytes(raw_data[10], raw_data[11]);
    packet->gyro_z_raw = MPU6050_PacketCombineBytes(raw_data[12], raw_data[13]);

    MPU6050_PacketUpdateScaledValues(packet);
    return 1U;
}
