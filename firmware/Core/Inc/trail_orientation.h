#ifndef TRAIL_ORIENTATION_H
#define TRAIL_ORIENTATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "hm10_packet.h"
#include "mpu6050_packet.h"

#define TRAIL_ORIENTATION_AXIS_COUNT              3U
#define TRAIL_ORIENTATION_X_AXIS                  0U
#define TRAIL_ORIENTATION_Y_AXIS                  1U
#define TRAIL_ORIENTATION_Z_AXIS                  2U

#define TRAIL_ORIENTATION_FULL_ROTATION_DEGREES   360.0f
#define TRAIL_ORIENTATION_HALF_ROTATION_DEGREES   180.0f

#define TRAIL_ORIENTATION_PHONE_CUBOID_WIDTH      2.0f
#define TRAIL_ORIENTATION_PHONE_CUBOID_DEPTH      1.0f
#define TRAIL_ORIENTATION_PHONE_CUBOID_HEIGHT     4.0f
#define TRAIL_ORIENTATION_PHONE_CUBOID_EDGE_COUNT 12U

/**
 * @brief Lists the possible status values returned by the trail orientation library.
 *
 * Values:
 * - TRAIL_ORIENTATION_OK: Operation completed successfully.
 * - TRAIL_ORIENTATION_ERROR: Operation failed because at least one argument was invalid.
 */
typedef enum
{
    TRAIL_ORIENTATION_OK = 0U,
    TRAIL_ORIENTATION_ERROR
} TrailOrientation_StatusTypeDef;

/**
 * @brief Stores one 3D point of the future phone model.
 *
 * Fields:
 * - x: Width-axis coordinate. Expected unit is selected by the renderer; value
 *      should be 0 or strictly positive.
 * - y: Depth-axis coordinate. Expected unit is selected by the renderer; value
 *      should be 0 or strictly positive.
 * - z: Height-axis coordinate. Expected unit is selected by the renderer; value
 *      should be 0 or strictly positive.
 */
typedef struct
{
    float x;
    float y;
    float z;
} TrailOrientation_Point3D;

/**
 * @brief Stores one edge of the future phone model as two 3D points.
 *
 * Fields:
 * - start: First point of the edge. Coordinates should be 0 or strictly positive.
 * - end: Second point of the edge. Coordinates should be 0 or strictly positive.
 */
typedef struct
{
    TrailOrientation_Point3D start;
    TrailOrientation_Point3D end;
} TrailOrientation_Edge3D;

extern volatile float TrailOrientation_PhoneRotation[TRAIL_ORIENTATION_AXIS_COUNT];
extern volatile float TrailOrientation_GyroscopeRotation[TRAIL_ORIENTATION_AXIS_COUNT];

/**
 * @brief Stores phone rotation values in the shared phone rotation array.
 * @param x_degrees Phone rotation around the x axis, in degrees.
 * @param y_degrees Phone rotation around the y axis, in degrees.
 * @param z_degrees Phone rotation around the z axis, in degrees.
 * @return TRAIL_ORIENTATION_OK after the values are stored.
 */
TrailOrientation_StatusTypeDef TrailOrientation_SetPhoneRotationDegrees(float x_degrees,
                                                                        float y_degrees,
                                                                        float z_degrees);

/**
 * @brief Updates phone rotation from one parsed HM-10 data packet.
 * @param packet Parsed HM-10 packet containing phone orientation as a
 *               quaternion. NULL is not allowed.
 * @return TRAIL_ORIENTATION_OK when the phone rotation array is updated;
 *         otherwise TRAIL_ORIENTATION_ERROR if packet is NULL or the
 *         quaternion is invalid.
 */
TrailOrientation_StatusTypeDef TrailOrientation_UpdatePhoneRotationFromPacket(const HM10_DataPacket* packet);

/**
 * @brief Stores gyroscope rotation values in the shared gyroscope rotation array.
 * @param x_degrees Gyroscope rotation around the x axis, in degrees.
 * @param y_degrees Gyroscope rotation around the y axis, in degrees.
 * @param z_degrees Gyroscope rotation around the z axis, in degrees.
 * @return TRAIL_ORIENTATION_OK after the values are stored.
 */
TrailOrientation_StatusTypeDef TrailOrientation_SetGyroscopeRotationDegrees(float x_degrees,
                                                                            float y_degrees,
                                                                            float z_degrees);

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
                                                                               float delta_time_seconds);

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
                                                                                  float delta_time_seconds);

/**
 * @brief Clears the shared gyroscope rotation array.
 * @return TRAIL_ORIENTATION_OK after the values are cleared.
 */
TrailOrientation_StatusTypeDef TrailOrientation_ResetGyroscopeRotation(void);

/**
 * @brief Calculates phone rotation relative to the gyroscope rotation.
 * @param relative_rotation Output array that receives x, y, and z relative
 *                          rotation values, in degrees. NULL is not allowed.
 * @return TRAIL_ORIENTATION_OK when the relative rotation is calculated;
 *         otherwise TRAIL_ORIENTATION_ERROR if relative_rotation is NULL.
 */
TrailOrientation_StatusTypeDef TrailOrientation_GetRelativeRotation(
    float relative_rotation[TRAIL_ORIENTATION_AXIS_COUNT]);

/**
 * @brief Updates relative phone orientation data and renders the rotated phone model.
 * @param mpu_packet Parsed MPU-6050 packet used to update gyroscope rotation.
 *                   NULL is not allowed.
 * @param hm10_packet Parsed HM-10 packet used to update phone rotation.
 *                    NULL is not allowed.
 * @param sensitivity_lsb_per_dps Gyroscope sensitivity, in LSB per degree per
 *                                second. Must be greater than 0.0f.
 * @param delta_time_seconds Time between the current and previous gyroscope
 *                           sample, in seconds. Must be greater than or equal
 *                           to 0.0f.
 * @param bounding_x Left x coordinate of the render bounding box, in pixels.
 * @param bounding_y Top y coordinate of the render bounding box, in pixels.
 * @param bounding_width Width of the render bounding box, in pixels.
 * @param bounding_height Height of the render bounding box, in pixels.
 * @param line_width Width of each rendered model edge, in pixels.
 * @param color ARGB color used to render the model edges.
 * @return TRAIL_ORIENTATION_OK when the model is updated and rendered;
 *         otherwise TRAIL_ORIENTATION_ERROR if an argument is invalid or the
 *         bounding box is too small.
 */
TrailOrientation_StatusTypeDef TrailOrientation_RenderRelativePhoneModel(const MPU6050_DataPacket* mpu_packet,
                                                                         const HM10_DataPacket* hm10_packet,
                                                                         float sensitivity_lsb_per_dps,
                                                                         float delta_time_seconds,
                                                                         uint32_t bounding_x,
                                                                         uint32_t bounding_y,
                                                                         uint32_t bounding_width,
                                                                         uint32_t bounding_height,
                                                                         uint32_t line_width,
                                                                         uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* TRAIL_ORIENTATION_H */
