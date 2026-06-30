#include "trail_orientation.h"

#include "trail_gui.h"

#include <math.h>

/**
 * Common orientation basis used by this module:
 * - x axis: phone model width
 * - y axis: phone model depth
 * - z axis: phone model height
 *
 * If the phone app or MPU-6050 module uses a different physical axis order,
 * adjust only the source-axis and sign constants below. The public algorithms
 * can stay unchanged.
 */
#define TRAIL_ORIENTATION_PHONE_COMMON_X_SOURCE_AXIS    TRAIL_ORIENTATION_X_AXIS
#define TRAIL_ORIENTATION_PHONE_COMMON_Y_SOURCE_AXIS    TRAIL_ORIENTATION_Y_AXIS
#define TRAIL_ORIENTATION_PHONE_COMMON_Z_SOURCE_AXIS    TRAIL_ORIENTATION_Z_AXIS

#define TRAIL_ORIENTATION_PHONE_COMMON_X_SIGN           1.0f
#define TRAIL_ORIENTATION_PHONE_COMMON_Y_SIGN           1.0f
#define TRAIL_ORIENTATION_PHONE_COMMON_Z_SIGN           1.0f

#define TRAIL_ORIENTATION_MPU6050_COMMON_X_SOURCE_AXIS  TRAIL_ORIENTATION_X_AXIS
#define TRAIL_ORIENTATION_MPU6050_COMMON_Y_SOURCE_AXIS  TRAIL_ORIENTATION_Y_AXIS
#define TRAIL_ORIENTATION_MPU6050_COMMON_Z_SOURCE_AXIS  TRAIL_ORIENTATION_Z_AXIS

#define TRAIL_ORIENTATION_MPU6050_COMMON_X_SIGN         1.0f
#define TRAIL_ORIENTATION_MPU6050_COMMON_Y_SIGN         1.0f
#define TRAIL_ORIENTATION_MPU6050_COMMON_Z_SIGN         1.0f

#define TRAIL_ORIENTATION_RADIANS_TO_DEGREES            57.29577951308232f

volatile float TrailOrientation_PhoneRotation[TRAIL_ORIENTATION_AXIS_COUNT] = {0.0f, 0.0f, 0.0f};
volatile float TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_AXIS_COUNT] = {0.0f, 0.0f, 0.0f};

static const TrailOrientation_Edge3D TrailOrientation_PhoneCuboidEdges[TRAIL_ORIENTATION_PHONE_CUBOID_EDGE_COUNT] =
{
    /* Bottom face, z = 0.0f. */
    {{0.0f, 0.0f, 0.0f}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, 0.0f}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, 0.0f}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}, {0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}},
    {{0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}, {0.0f, 0.0f, 0.0f}},

    /* Top face, z = 4.0f. */
    {{0.0f, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}, {0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}, {0.0f, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},

    /* Vertical edges. */
    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, 0.0f}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, 0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}, {TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}},
    {{0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, 0.0f}, {0.0f, TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH, TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT}}
};

/**
 * @brief Limits a floating-point value to the provided inclusive range.
 * @param value Value to clamp.
 * @param minimum Lowest allowed value.
 * @param maximum Highest allowed value.
 * @return minimum when value is below the range, maximum when value is above
 *         the range, otherwise value unchanged.
 */
static float TrailOrientation_ClampFloat(float value,
                                         float minimum,
                                         float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

/**
 * @brief Normalizes one angle to the module's configured rotation interval.
 * @param degrees Input angle, in degrees.
 * @return Equivalent angle normalized to the interval
 *         (-TRAIL_ORIENTATION_HALF_ROTATION_DEGREES,
 *         TRAIL_ORIENTATION_HALF_ROTATION_DEGREES].
 */
static float TrailOrientation_NormalizeDegrees(float degrees)
{
    while (degrees > TRAIL_ORIENTATION_HALF_ROTATION_DEGREES)
    {
        degrees -= TRAIL_ORIENTATION_FULL_ROTATION_DEGREES;
    }

    while (degrees <= -TRAIL_ORIENTATION_HALF_ROTATION_DEGREES)
    {
        degrees += TRAIL_ORIENTATION_FULL_ROTATION_DEGREES;
    }

    return degrees;
}

/**
 * @brief Stores normalized x, y, and z rotation values in a shared rotation array.
 * @param rotation Output rotation array to update. NULL is not allowed.
 * @param x_degrees Rotation around the x axis, in degrees.
 * @param y_degrees Rotation around the y axis, in degrees.
 * @param z_degrees Rotation around the z axis, in degrees.
 * @return None.
 */
static void TrailOrientation_StoreRotation(volatile float rotation[TRAIL_ORIENTATION_AXIS_COUNT],
                                           float x_degrees,
                                           float y_degrees,
                                           float z_degrees)
{
    rotation[TRAIL_ORIENTATION_X_AXIS] = TrailOrientation_NormalizeDegrees(x_degrees);
    rotation[TRAIL_ORIENTATION_Y_AXIS] = TrailOrientation_NormalizeDegrees(y_degrees);
    rotation[TRAIL_ORIENTATION_Z_AXIS] = TrailOrientation_NormalizeDegrees(z_degrees);
}

/**
 * @brief Converts phone orientation from a parsed HM-10 quaternion to Euler angles.
 * @param packet Parsed HM-10 packet containing phone orientation as a
 *               quaternion. NULL is not allowed.
 * @param euler_degrees Output array that receives x, y, and z Euler angles,
 *                      in degrees. NULL is not allowed.
 * @return TRAIL_ORIENTATION_OK when the quaternion is converted; otherwise
 *         TRAIL_ORIENTATION_ERROR if packet, euler_degrees, or the quaternion
 *         magnitude is invalid.
 */
static TrailOrientation_StatusTypeDef TrailOrientation_ConvertQuaternionToEulerDegrees(const HM10_DataPacket* packet,
                                                                                       float euler_degrees[TRAIL_ORIENTATION_AXIS_COUNT])
{
    float qw;
    float qx;
    float qy;
    float qz;
    float magnitude;
    float sinr_cosp;
    float cosr_cosp;
    float sinp;
    float siny_cosp;
    float cosy_cosp;

    if ((packet == NULL) || (euler_degrees == NULL))
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    qw = (float)packet->qw;
    qx = (float)packet->qx;
    qy = (float)packet->qy;
    qz = (float)packet->qz;

    magnitude = sqrtf((qw * qw) + (qx * qx) + (qy * qy) + (qz * qz));

    if (magnitude <= 0.0f)
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    qw /= magnitude;
    qx /= magnitude;
    qy /= magnitude;
    qz /= magnitude;

    /* Roll: rotation around x axis. */
    sinr_cosp = 2.0f * ((qw * qx) + (qy * qz));
    cosr_cosp = 1.0f - (2.0f * ((qx * qx) + (qy * qy)));
    euler_degrees[TRAIL_ORIENTATION_X_AXIS] =
        atan2f(sinr_cosp, cosr_cosp) * TRAIL_ORIENTATION_RADIANS_TO_DEGREES;

    /* Pitch: rotation around y axis. */
    sinp = 2.0f * ((qw * qy) - (qz * qx));
    sinp = TrailOrientation_ClampFloat(sinp, -1.0f, 1.0f);
    euler_degrees[TRAIL_ORIENTATION_Y_AXIS] =
        asinf(sinp) * TRAIL_ORIENTATION_RADIANS_TO_DEGREES;

    /* Yaw: rotation around z axis. */
    siny_cosp = 2.0f * ((qw * qz) + (qx * qy));
    cosy_cosp = 1.0f - (2.0f * ((qy * qy) + (qz * qz)));
    euler_degrees[TRAIL_ORIENTATION_Z_AXIS] =
        atan2f(siny_cosp, cosy_cosp) * TRAIL_ORIENTATION_RADIANS_TO_DEGREES;

    return TRAIL_ORIENTATION_OK;
}

/**
 * @brief Reads one raw gyroscope axis from a parsed MPU-6050 packet.
 * @param packet Parsed MPU-6050 packet. NULL is not allowed.
 * @param axis Requested axis index. Expected value is TRAIL_ORIENTATION_X_AXIS,
 *             TRAIL_ORIENTATION_Y_AXIS, or TRAIL_ORIENTATION_Z_AXIS.
 * @return Raw gyroscope value for the selected axis, in sensor LSB, or 0.0f
 *         when axis is invalid.
 */
static float TrailOrientation_ReadGyroscopeAxis(const MPU6050_DataPacket* packet,
                                                uint32_t axis)
{
    float value = 0.0f;

    switch (axis)
    {
        case TRAIL_ORIENTATION_X_AXIS:
            value = (float)packet->gyro_x_raw;
            break;

        case TRAIL_ORIENTATION_Y_AXIS:
            value = (float)packet->gyro_y_raw;
            break;

        case TRAIL_ORIENTATION_Z_AXIS:
            value = (float)packet->gyro_z_raw;
            break;

        default:
            value = 0.0f;
            break;
    }

    return value;
}

/**
 * @brief Stores phone rotation values in the shared phone rotation array.
 * @param x_degrees Phone rotation around the x axis, in degrees.
 * @param y_degrees Phone rotation around the y axis, in degrees.
 * @param z_degrees Phone rotation around the z axis, in degrees.
 * @return TRAIL_ORIENTATION_OK after the values are stored.
 */
TrailOrientation_StatusTypeDef TrailOrientation_SetPhoneRotationDegrees(float x_degrees,
                                                                        float y_degrees,
                                                                        float z_degrees)
{
    TrailOrientation_StoreRotation(TrailOrientation_PhoneRotation,
                                   x_degrees,
                                   y_degrees,
                                   z_degrees);

    return TRAIL_ORIENTATION_OK;
}

/**
 * @brief Updates phone rotation from one parsed HM-10 data packet.
 * @param packet Parsed HM-10 packet containing phone orientation as a
 *               quaternion. NULL is not allowed.
 * @return TRAIL_ORIENTATION_OK when the phone rotation array is updated;
 *         otherwise TRAIL_ORIENTATION_ERROR if packet is NULL or the
 *         quaternion is invalid.
 */
TrailOrientation_StatusTypeDef TrailOrientation_UpdatePhoneRotationFromPacket(const HM10_DataPacket* packet)
{
    float euler_degrees[TRAIL_ORIENTATION_AXIS_COUNT];
    float x_degrees;
    float y_degrees;
    float z_degrees;

    if (TrailOrientation_ConvertQuaternionToEulerDegrees(packet, euler_degrees) != TRAIL_ORIENTATION_OK)
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    x_degrees = euler_degrees[TRAIL_ORIENTATION_PHONE_COMMON_X_SOURCE_AXIS]
        * TRAIL_ORIENTATION_PHONE_COMMON_X_SIGN;

    y_degrees = euler_degrees[TRAIL_ORIENTATION_PHONE_COMMON_Y_SOURCE_AXIS]
        * TRAIL_ORIENTATION_PHONE_COMMON_Y_SIGN;

    z_degrees = euler_degrees[TRAIL_ORIENTATION_PHONE_COMMON_Z_SOURCE_AXIS]
        * TRAIL_ORIENTATION_PHONE_COMMON_Z_SIGN;

    return TrailOrientation_SetPhoneRotationDegrees(x_degrees,
                                                    y_degrees,
                                                    z_degrees);
}

/**
 * @brief Stores gyroscope rotation values in the shared gyroscope rotation array.
 * @param x_degrees Gyroscope rotation around the x axis, in degrees.
 * @param y_degrees Gyroscope rotation around the y axis, in degrees.
 * @param z_degrees Gyroscope rotation around the z axis, in degrees.
 * @return TRAIL_ORIENTATION_OK after the values are stored.
 */
TrailOrientation_StatusTypeDef TrailOrientation_SetGyroscopeRotationDegrees(float x_degrees,
                                                                            float y_degrees,
                                                                            float z_degrees)
{
    TrailOrientation_StoreRotation(TrailOrientation_GyroscopeRotation,
                                   x_degrees,
                                   y_degrees,
                                   z_degrees);

    return TRAIL_ORIENTATION_OK;
}

/**
 * @brief Converts raw gyroscope angular velocity data into accumulated rotation.
 * @param raw_x Raw gyroscope value for the x axis, in sensor LSB.
 * @param raw_y Raw gyroscope value for the y axis, in sensor LSB.
 * @param raw_z Raw gyroscope value for the z axis, in sensor LSB.
 * @param sensitivity_lsb_per_dps Gyroscope sensitivity, in LSB per degree per
 *                                second. Must be greater than 0.0f.
 * @param delta_time_seconds Time between the current and previous sample, in
 *                           seconds. Must be greater than or equal to 0.0f.
 * @return TRAIL_ORIENTATION_OK when the rotation is updated; otherwise
 *         TRAIL_ORIENTATION_ERROR if sensitivity_lsb_per_dps or
 *         delta_time_seconds is invalid.
 */
TrailOrientation_StatusTypeDef TrailOrientation_UpdateGyroscopeRotationFromRaw(int16_t raw_x,
                                                                               int16_t raw_y,
                                                                               int16_t raw_z,
                                                                               float sensitivity_lsb_per_dps,
                                                                               float delta_time_seconds)
{
    float x_degrees;
    float y_degrees;
    float z_degrees;

    if ((sensitivity_lsb_per_dps <= 0.0f) || (delta_time_seconds < 0.0f))
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    x_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_X_AXIS]
        + (((float)raw_x / sensitivity_lsb_per_dps) * delta_time_seconds);
    y_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Y_AXIS]
        + (((float)raw_y / sensitivity_lsb_per_dps) * delta_time_seconds);
    z_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Z_AXIS]
        + (((float)raw_z / sensitivity_lsb_per_dps) * delta_time_seconds);

    TrailOrientation_StoreRotation(TrailOrientation_GyroscopeRotation,
                                   x_degrees,
                                   y_degrees,
                                   z_degrees);

    return TRAIL_ORIENTATION_OK;
}

/**
 * @brief Updates gyroscope rotation from one parsed MPU-6050 data packet.
 * @param packet Parsed MPU-6050 packet containing raw gyroscope x, y, and z
 *               values, in sensor LSB. NULL is not allowed.
 * @param sensitivity_lsb_per_dps Gyroscope sensitivity, in LSB per degree per
 *                                second. Must be greater than 0.0f.
 * @param delta_time_seconds Time between the current and previous sample, in
 *                           seconds. Must be greater than or equal to 0.0f.
 * @return TRAIL_ORIENTATION_OK when the gyroscope rotation array is updated;
 *         otherwise TRAIL_ORIENTATION_ERROR if packet is NULL or a conversion
 *         argument is invalid.
 */
TrailOrientation_StatusTypeDef TrailOrientation_UpdateGyroscopeRotationFromPacket(const MPU6050_DataPacket* packet,
                                                                                  float sensitivity_lsb_per_dps,
                                                                                  float delta_time_seconds)
{
    float raw_x;
    float raw_y;
    float raw_z;
    float x_degrees;
    float y_degrees;
    float z_degrees;

    if ((packet == NULL) || (sensitivity_lsb_per_dps <= 0.0f) || (delta_time_seconds < 0.0f))
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    raw_x = TrailOrientation_ReadGyroscopeAxis(packet,
                                               TRAIL_ORIENTATION_MPU6050_COMMON_X_SOURCE_AXIS)
        * TRAIL_ORIENTATION_MPU6050_COMMON_X_SIGN;

    raw_y = TrailOrientation_ReadGyroscopeAxis(packet,
                                               TRAIL_ORIENTATION_MPU6050_COMMON_Y_SOURCE_AXIS)
        * TRAIL_ORIENTATION_MPU6050_COMMON_Y_SIGN;

    raw_z = TrailOrientation_ReadGyroscopeAxis(packet,
                                               TRAIL_ORIENTATION_MPU6050_COMMON_Z_SOURCE_AXIS)
        * TRAIL_ORIENTATION_MPU6050_COMMON_Z_SIGN;

    x_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_X_AXIS]
        + ((raw_x / sensitivity_lsb_per_dps) * delta_time_seconds);
    y_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Y_AXIS]
        + ((raw_y / sensitivity_lsb_per_dps) * delta_time_seconds);
    z_degrees = TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Z_AXIS]
        + ((raw_z / sensitivity_lsb_per_dps) * delta_time_seconds);

    return TrailOrientation_SetGyroscopeRotationDegrees(x_degrees,
                                                        y_degrees,
                                                        z_degrees);
}

/**
 * @brief Clears the shared gyroscope rotation array.
 * @return TRAIL_ORIENTATION_OK after the values are cleared.
 */
TrailOrientation_StatusTypeDef TrailOrientation_ResetGyroscopeRotation(void)
{
    TrailOrientation_StoreRotation(TrailOrientation_GyroscopeRotation,
                                   0.0f,
                                   0.0f,
                                   0.0f);

    return TRAIL_ORIENTATION_OK;
}

/**
 * @brief Calculates phone rotation relative to the gyroscope rotation.
 * @param relative_rotation Output array that receives x, y, and z relative
 *                          rotation values, in degrees. NULL is not allowed.
 * @return TRAIL_ORIENTATION_OK when the relative rotation is calculated;
 *         otherwise TRAIL_ORIENTATION_ERROR if relative_rotation is NULL.
 */
TrailOrientation_StatusTypeDef TrailOrientation_GetRelativeRotation(float relative_rotation[TRAIL_ORIENTATION_AXIS_COUNT])
{
    if (relative_rotation == NULL)
    {
        return TRAIL_ORIENTATION_ERROR;
    }

    relative_rotation[TRAIL_ORIENTATION_X_AXIS] = TrailOrientation_NormalizeDegrees(
        TrailOrientation_PhoneRotation[TRAIL_ORIENTATION_X_AXIS]
        - TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_X_AXIS]);

    relative_rotation[TRAIL_ORIENTATION_Y_AXIS] = TrailOrientation_NormalizeDegrees(
        TrailOrientation_PhoneRotation[TRAIL_ORIENTATION_Y_AXIS]
        - TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Y_AXIS]);

    relative_rotation[TRAIL_ORIENTATION_Z_AXIS] = TrailOrientation_NormalizeDegrees(
        TrailOrientation_PhoneRotation[TRAIL_ORIENTATION_Z_AXIS]
        - TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_Z_AXIS]);

    return TRAIL_ORIENTATION_OK;
}