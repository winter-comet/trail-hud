#include "trail_gui.h"
#include "debug_terminal.h"

#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Shared math ---------------------------------------------------------------*/
#define TRAIL_GUI_TITLE "TRAIL-HUD"
#define TRAIL_GUI_DEG_TO_RAD 0.01745329251994329577f
#define TRAIL_GUI_RAD_TO_DEG 57.29577951308232087680f

/* Phone render --------------------------------------------------------------*/
#define TRAIL_GUI_PHONE_MODEL_HALF_WIDTH 0.38f
#define TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT 0.80f
#define TRAIL_GUI_PHONE_MODEL_HALF_DEPTH 0.055f
#define TRAIL_GUI_PHONE_MODEL_RADIUS_RATIO 0.38f
#define TRAIL_GUI_QUATERNION_EPSILON 0.000001f

/* Tilt indicator widget -----------------------------------------------------*/
#define TRAIL_GUI_TILT_DIAL_RADIUS_RATIO 0.32f
#define TRAIL_GUI_TILT_DATUM_LENGTH_RATIO 0.10f
#define TRAIL_GUI_TILT_DATUM_GAP_RATIO 0.05f
#define TRAIL_GUI_TILT_PIVOT_RADIUS_PX 5U
#define TRAIL_GUI_TILT_READOUT_GAP_PX 14U
#define TRAIL_GUI_TILT_READOUT_DECIMALS 1U
#define TRAIL_GUI_TILT_RING_RADIUS_PX 4U
#define TRAIL_GUI_TILT_RING_GAP_PX 6U

/* Loading screen ------------------------------------------------------------*/
#define TRAIL_GUI_LOADING_TITLE_X 48U
#define TRAIL_GUI_LOADING_TITLE_Y 92U
#define TRAIL_GUI_LOADING_BAR_X 44U
#define TRAIL_GUI_LOADING_BAR_Y 138U
#define TRAIL_GUI_LOADING_BAR_WIDTH 392U
#define TRAIL_GUI_LOADING_BAR_HEIGHT 30U
#define TRAIL_GUI_LOADING_BAR_BORDER_WIDTH 2U
#define TRAIL_GUI_LOADING_BAR_PADDING 6U

typedef struct
{
    float x;
    float y;
    float z;
} TrailGui_Vector3;

typedef struct
{
    float m[3][3];
} TrailGui_Matrix3;

typedef struct
{
    float w;
    float x;
    float y;
    float z;
} TrailGui_Quaternion;

/* Scalar helpers */
static int32_t TrailGui_Abs32(int32_t value);
static uint16_t TrailGui_MinUint16(uint16_t lhs, uint16_t rhs);
static int32_t TrailGui_RoundFloatToInt32(float value);
static uint16_t TrailGui_ClampInt32ToUint16(int32_t value, uint16_t min_value, uint16_t max_value);

/* Bounding box helpers */
static uint8_t TrailGui_NormalizeAndClipBoundingBox(TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxWidth(const TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxHeight(const TrailGui_BoundingBox* bounding_box);

/* Drawing primitives */
static void TrailGui_FillLinePoint(int32_t center_x, int32_t center_y, uint16_t width, uint32_t color);
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px, uint16_t width_px, uint16_t height_px);
static uint16_t TrailGui_ClampRadius(uint16_t radius_px, uint16_t width_px, uint16_t height_px);
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px, uint16_t distance_from_corner_center_px);

/* Rotation math */
static TrailGui_Quaternion TrailGui_QuaternionIdentity(void);
static TrailGui_Quaternion TrailGui_QuaternionNormalize(TrailGui_Quaternion quaternion);
static TrailGui_Matrix3 TrailGui_QuaternionToRotationMatrix(TrailGui_Quaternion quaternion);
static TrailGui_Vector3 TrailGui_Mat3RotateVector(const TrailGui_Matrix3* matrix, TrailGui_Vector3 vector);

/* Phone widgets */
static TrailGui_Quaternion TrailGui_BuildPhoneQuaternion(const HM10_DataPacket* hm10_packet);

/* Tilt indicator widget */
static void TrailGui_DrawTiltReadout(TrailGui_BoundingBox bounding_box,
                                     uint16_t top_y,
                                     float tilt_deg,
                                     uint16_t line_width,
                                     uint32_t color,
                                     uint32_t background_color);

/* Scalar helpers ------------------------------------------------------------*/

/**
 * @brief Returns the absolute value of a signed 32-bit integer.
 * @param value Signed 32-bit integer input value.
 * @return Absolute value of value.
 */
static int32_t TrailGui_Abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

/**
 * @brief Returns the smaller of two unsigned 16-bit integers.
 * @param lhs First value to compare.
 * @param rhs Second value to compare.
 * @return lhs when lhs is smaller than rhs; otherwise rhs.
 */
static uint16_t TrailGui_MinUint16(uint16_t lhs, uint16_t rhs)
{
    return (lhs < rhs) ? lhs : rhs;
}

/**
 * @brief Rounds a floating-point value to the nearest signed 32-bit integer.
 * @param value Floating-point value to round.
 * @return Nearest signed 32-bit integer.
 */
static int32_t TrailGui_RoundFloatToInt32(float value)
{
    return (value >= 0.0f) ? (int32_t)(value + 0.5f) : (int32_t)(value - 0.5f);
}

/**
 * @brief Clamps a signed coordinate to an inclusive unsigned pixel interval.
 * @param value Signed coordinate to clamp.
 * @param min_value Minimum accepted coordinate.
 * @param max_value Maximum accepted coordinate.
 * @return value clamped to [min_value, max_value].
 */
static uint16_t TrailGui_ClampInt32ToUint16(int32_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < (int32_t)min_value)
    {
        return min_value;
    }

    if (value > (int32_t)max_value)
    {
        return max_value;
    }

    return (uint16_t)value;
}

/* Bounding box helpers ------------------------------------------------------*/

/**
 * @brief Normalizes and clips a bounding box to the LCD screen area.
 * @param bounding_box Pointer to the bounding box to normalize and clip. NULL
 *                     is not allowed.
 * @return 1 when the clipped bounding box is drawable; otherwise 0.
 */
static uint8_t TrailGui_NormalizeAndClipBoundingBox(TrailGui_BoundingBox* bounding_box)
{
    uint16_t temp;

    if (bounding_box == NULL)
    {
        return 0U;
    }

    if (bounding_box->x_min > bounding_box->x_max)
    {
        temp = bounding_box->x_min;
        bounding_box->x_min = bounding_box->x_max;
        bounding_box->x_max = temp;
    }

    if (bounding_box->y_min > bounding_box->y_max)
    {
        temp = bounding_box->y_min;
        bounding_box->y_min = bounding_box->y_max;
        bounding_box->y_max = temp;
    }

    if ((bounding_box->x_min >= TRAIL_GUI_SCREEN_WIDTH) ||
        (bounding_box->y_min >= TRAIL_GUI_SCREEN_HEIGHT))
    {
        return 0U;
    }

    if (bounding_box->x_max >= TRAIL_GUI_SCREEN_WIDTH)
    {
        bounding_box->x_max = TRAIL_GUI_SCREEN_WIDTH - 1U;
    }

    if (bounding_box->y_max >= TRAIL_GUI_SCREEN_HEIGHT)
    {
        bounding_box->y_max = TRAIL_GUI_SCREEN_HEIGHT - 1U;
    }

    return 1U;
}

/**
 * @brief Calculates the inclusive width of a bounding box in pixels.
 * @param bounding_box Pointer to a normalized bounding box. The pointer must
 *                     not be NULL, and x_max is expected to be greater than or
 *                     equal to x_min.
 * @return Bounding box width in pixels.
 */
static uint16_t TrailGui_GetBoundingBoxWidth(const TrailGui_BoundingBox* bounding_box)
{
    return (uint16_t)(bounding_box->x_max - bounding_box->x_min + 1U);
}

/**
 * @brief Calculates the inclusive height of a bounding box in pixels.
 * @param bounding_box Pointer to a normalized bounding box. The pointer must
 *                     not be NULL, and y_max is expected to be greater than or
 *                     equal to y_min.
 * @return Bounding box height in pixels.
 */
static uint16_t TrailGui_GetBoundingBoxHeight(const TrailGui_BoundingBox* bounding_box)
{
    return (uint16_t)(bounding_box->y_max - bounding_box->y_min + 1U);
}

/* Drawing primitives --------------------------------------------------------*/

/**
 * @brief Draws one clipped square sample used to create a thick line.
 * @param center_x Horizontal center of the square sample in LCD pixel
 *                 coordinates. Values outside the screen are allowed and are
 *                 clipped internally.
 * @param center_y Vertical center of the square sample in LCD pixel
 *                 coordinates. Values outside the screen are allowed and are
 *                 clipped internally.
 * @param width Width and height of the square sample in pixels. A value of 0
 *              leaves the screen unchanged.
 * @param color ARGB8888 LCD color value used to fill the square sample.
 * @return None.
 */
static void TrailGui_FillLinePoint(int32_t center_x, int32_t center_y, uint16_t width, uint32_t color)
{
    int32_t x;
    int32_t y;
    int32_t rect_width;
    int32_t rect_height;

    if (width == 0U)
    {
        return;
    }

    rect_width = (int32_t)width;
    rect_height = (int32_t)width;
    x = center_x - ((int32_t)width / 2);
    y = center_y - ((int32_t)width / 2);

    if ((x >= (int32_t)TRAIL_GUI_SCREEN_WIDTH) || (y >= (int32_t)TRAIL_GUI_SCREEN_HEIGHT))
    {
        return;
    }

    if (((x + rect_width) <= 0) || ((y + rect_height) <= 0))
    {
        return;
    }

    if (x < 0)
    {
        rect_width += x;
        x = 0;
    }

    if (y < 0)
    {
        rect_height += y;
        y = 0;
    }

    if ((x + rect_width) > (int32_t)TRAIL_GUI_SCREEN_WIDTH)
    {
        rect_width = (int32_t)TRAIL_GUI_SCREEN_WIDTH - x;
    }

    if ((y + rect_height) > (int32_t)TRAIL_GUI_SCREEN_HEIGHT)
    {
        rect_height = (int32_t)TRAIL_GUI_SCREEN_HEIGHT - y;
    }

    if ((rect_width <= 0) || (rect_height <= 0))
    {
        return;
    }

    UTIL_LCD_FillRect((uint32_t)x, (uint32_t)y, (uint32_t)rect_width, (uint32_t)rect_height, color);
}

/**
 * @brief Draws a straight line segment between two points.
 * @param start Start point of the line in LCD pixel coordinates.
 * @param end End point of the line in LCD pixel coordinates.
 * @param width Line thickness in pixels. A value of 0 leaves the screen
 *              unchanged.
 * @param color ARGB8888 LCD color value used to draw the line.
 * @return None.
 */
void TrailGui_DrawLine(TrailGui_Point start, TrailGui_Point end, uint16_t width, uint32_t color)
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t dx;
    int32_t dy;
    int32_t sx;
    int32_t sy;
    int32_t error;
    int32_t error2;

    if (width == 0U)
    {
        return;
    }

    x0 = (int32_t)start.x;
    y0 = (int32_t)start.y;
    x1 = (int32_t)end.x;
    y1 = (int32_t)end.y;
    dx = TrailGui_Abs32(x1 - x0);
    dy = -TrailGui_Abs32(y1 - y0);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    error = dx + dy;

    for (;;)
    {
        TrailGui_FillLinePoint(x0, y0, width, color);

        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        error2 = 2 * error;

        if (error2 >= dy)
        {
            error += dy;
            x0 += sx;
        }

        if (error2 <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief Clamps a corner edge length so opposite corners do not overlap.
 * @param corner_length_px Requested corner edge length in pixels.
 * @param width_px Width of the target bounding box in pixels.
 * @param height_px Height of the target bounding box in pixels.
 * @return Clamped corner edge length in pixels.
 */
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px, uint16_t width_px, uint16_t height_px)
{
    uint16_t max_length = width_px / 2U;

    if ((height_px / 2U) < max_length)
    {
        max_length = height_px / 2U;
    }

    if (corner_length_px > max_length)
    {
        return max_length;
    }

    return corner_length_px;
}

/**
 * @brief Draws a corner-only rectangle inside the supplied bounding box.
 * @param bounding_box Rectangle bounds in LCD pixels. x_min/y_min and x_max/y_max
 *                     are inclusive outer-edge coordinates. Reversed bounds are
 *                     normalized internally. Bounds outside the screen are
 *                     clipped.
 * @param corner_length_px Length of each visible corner edge in pixels. A value
 *                         of 0 leaves the screen unchanged.
 * @param color ARGB8888 LCD color value used for the corner lines.
 * @return None.
 */
void TrailGui_DrawBoundingRectangle(TrailGui_BoundingBox bounding_box,
                                    uint16_t corner_length_px,
                                    uint32_t color)
{
    uint16_t min_x;
    uint16_t max_x;
    uint16_t min_y;
    uint16_t max_y;
    uint16_t width_px;
    uint16_t height_px;
    uint16_t length_px;

    if (corner_length_px == 0U)
    {
        return;
    }

    if (TrailGui_NormalizeAndClipBoundingBox(&bounding_box) == 0U)
    {
        return;
    }

    width_px = TrailGui_GetBoundingBoxWidth(&bounding_box);
    height_px = TrailGui_GetBoundingBoxHeight(&bounding_box);

    if ((width_px < 2U) || (height_px < 2U))
    {
        return;
    }

    length_px = TrailGui_ClampCornerLength(corner_length_px, width_px, height_px);

    if (length_px == 0U)
    {
        return;
    }

    min_x = bounding_box.x_min;
    max_x = bounding_box.x_max;
    min_y = bounding_box.y_min;
    max_y = bounding_box.y_max;

    /* Top-left corner. */
    UTIL_LCD_DrawHLine(min_x, min_y, length_px, color);
    UTIL_LCD_DrawVLine(min_x, min_y, length_px, color);

    /* Top-right corner. */
    UTIL_LCD_DrawHLine((uint32_t)(max_x - length_px + 1U), min_y, length_px, color);
    UTIL_LCD_DrawVLine(max_x, min_y, length_px, color);

    /* Bottom-left corner. */
    UTIL_LCD_DrawHLine(min_x, max_y, length_px, color);
    UTIL_LCD_DrawVLine(min_x, (uint32_t)(max_y - length_px + 1U), length_px, color);

    /* Bottom-right corner. */
    UTIL_LCD_DrawHLine((uint32_t)(max_x - length_px + 1U), max_y, length_px, color);
    UTIL_LCD_DrawVLine(max_x, (uint32_t)(max_y - length_px + 1U), length_px, color);
}

/**
 * @brief Clamps a rounded-rectangle radius to fit inside the target rectangle.
 * @param radius_px Requested corner radius in pixels.
 * @param width_px Width of the target bounding box in pixels.
 * @param height_px Height of the target bounding box in pixels.
 * @return Clamped radius in pixels. Returns 0 when the rectangle is too small
 *         to draw rounded corners.
 */
static uint16_t TrailGui_ClampRadius(uint16_t radius_px, uint16_t width_px, uint16_t height_px)
{
    uint16_t max_radius;

    if ((width_px < 3U) || (height_px < 3U))
    {
        return 0U;
    }

    max_radius = (uint16_t)((width_px - 1U) / 2U);

    if (((height_px - 1U) / 2U) < max_radius)
    {
        max_radius = (uint16_t)((height_px - 1U) / 2U);
    }

    if (radius_px > max_radius)
    {
        return max_radius;
    }

    return radius_px;
}

/**
 * @brief Calculates the horizontal inset needed for one rounded-corner row.
 * @param radius_px Radius of the rounded corner in pixels. A value of 0 returns
 *                  an inset of 0.
 * @param distance_from_corner_center_px Vertical distance from the rounded
 *                                       corner center row in pixels.
 * @return Horizontal inset in pixels for the selected row.
 */
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px, uint16_t distance_from_corner_center_px)
{
    uint32_t radius_squared;
    uint32_t distance_squared;
    uint32_t x_extent;

    if (radius_px == 0U)
    {
        return 0U;
    }

    radius_squared = (uint32_t)radius_px * (uint32_t)radius_px;
    distance_squared = (uint32_t)distance_from_corner_center_px * (uint32_t)distance_from_corner_center_px;

    /*
     * x_extent is the largest integer such that
     * x_extent^2 + distance_squared <= radius_squared, i.e.
     * floor(sqrt(radius_squared - distance_squared)). Using sqrtf directly
     * (the FPU already does float sqrt for the quaternion math below) turns
     * this into one O(1) call instead of a per-pixel decrementing search.
     */
    if (distance_squared >= radius_squared)
    {
        return radius_px;
    }

    x_extent = (uint32_t)sqrtf((float)(radius_squared - distance_squared));

    return (uint16_t)((uint32_t)radius_px - x_extent);
}

/**
 * @brief Draws a filled rounded rectangle inside the supplied bounding box.
 * @param bounding_box Rectangle bounds in LCD pixels. x_min/y_min and x_max/y_max
 *                     are inclusive outer-edge coordinates. Reversed bounds are
 *                     normalized internally. Bounds outside the screen are
 *                     clipped. The drawn shape always stays inside this
 *                     bounding box.
 * @param radius_px Corner radius in pixels. Values larger than the useful half
 *                  extents of the rectangle are clamped automatically.
 * @param color ARGB8888 LCD color value used to fill the rounded rectangle.
 * @return None.
 */
void TrailGui_DrawRoundedRectangle(TrailGui_BoundingBox bounding_box, uint16_t radius_px, uint32_t color)
{
    uint16_t width_px;
    uint16_t height_px;
    uint16_t clamped_radius_px;
    uint16_t row;

    if (TrailGui_NormalizeAndClipBoundingBox(&bounding_box) == 0U)
    {
        return;
    }

    width_px = TrailGui_GetBoundingBoxWidth(&bounding_box);
    height_px = TrailGui_GetBoundingBoxHeight(&bounding_box);
    clamped_radius_px = TrailGui_ClampRadius(radius_px, width_px, height_px);

    if (clamped_radius_px == 0U)
    {
        UTIL_LCD_FillRect(bounding_box.x_min, bounding_box.y_min, width_px, height_px, color);
        return;
    }

    for (row = 0U; row < height_px; row++)
    {
        uint16_t inset_px = 0U;
        uint16_t distance_px;
        uint16_t line_width_px;

        if (row < clamped_radius_px)
        {
            distance_px = (uint16_t)(clamped_radius_px - row);
            inset_px = TrailGui_CircleInsetForRow(clamped_radius_px, distance_px);
        }
        else if (row > (uint16_t)(height_px - clamped_radius_px - 1U))
        {
            distance_px = (uint16_t)(row - (height_px - clamped_radius_px - 1U));
            inset_px = TrailGui_CircleInsetForRow(clamped_radius_px, distance_px);
        }

        line_width_px = (uint16_t)(width_px - (2U * inset_px));
        UTIL_LCD_DrawHLine((uint32_t)(bounding_box.x_min + inset_px),
                           (uint32_t)(bounding_box.y_min + row),
                           line_width_px,
                           color);
    }
}

/* Rotation math -------------------------------------------------------------*/

/**
 * @brief Returns the identity quaternion.
 * @param None.
 * @return Identity quaternion with no rotation.
 */
static TrailGui_Quaternion TrailGui_QuaternionIdentity(void)
{
    TrailGui_Quaternion quaternion = {1.0f, 0.0f, 0.0f, 0.0f};

    return quaternion;
}

/**
 * @brief Normalizes a quaternion for stable vector rotation.
 * @param quaternion Quaternion to normalize.
 * @return Normalized quaternion, or identity when the input magnitude is too
 *         small.
 */
static TrailGui_Quaternion TrailGui_QuaternionNormalize(TrailGui_Quaternion quaternion)
{
    float length_squared = (quaternion.w * quaternion.w) +
        (quaternion.x * quaternion.x) +
        (quaternion.y * quaternion.y) +
        (quaternion.z * quaternion.z);
    float inv_length;

    if (length_squared <= TRAIL_GUI_QUATERNION_EPSILON)
    {
        return TrailGui_QuaternionIdentity();
    }

    inv_length = 1.0f / sqrtf(length_squared);
    quaternion.w *= inv_length;
    quaternion.x *= inv_length;
    quaternion.y *= inv_length;
    quaternion.z *= inv_length;

    return quaternion;
}

/**
 * @brief Converts a unit quaternion into an equivalent 3x3 rotation matrix.
 * @param quaternion Unit quaternion to convert; need not be pre-normalized.
 * @return Rotation matrix equivalent to quaternion, computed once so it can be
 *         reused across multiple vector rotations without repeating
 *         quaternion algebra per vertex.
 */
static TrailGui_Matrix3 TrailGui_QuaternionToRotationMatrix(TrailGui_Quaternion quaternion)
{
    TrailGui_Matrix3 matrix;
    float xx;
    float yy;
    float zz;
    float xy;
    float xz;
    float yz;
    float wx;
    float wy;
    float wz;

    quaternion = TrailGui_QuaternionNormalize(quaternion);

    xx = quaternion.x * quaternion.x;
    yy = quaternion.y * quaternion.y;
    zz = quaternion.z * quaternion.z;
    xy = quaternion.x * quaternion.y;
    xz = quaternion.x * quaternion.z;
    yz = quaternion.y * quaternion.z;
    wx = quaternion.w * quaternion.x;
    wy = quaternion.w * quaternion.y;
    wz = quaternion.w * quaternion.z;

    matrix.m[0][0] = 1.0f - (2.0f * (yy + zz));
    matrix.m[0][1] = 2.0f * (xy - wz);
    matrix.m[0][2] = 2.0f * (xz + wy);

    matrix.m[1][0] = 2.0f * (xy + wz);
    matrix.m[1][1] = 1.0f - (2.0f * (xx + zz));
    matrix.m[1][2] = 2.0f * (yz - wx);

    matrix.m[2][0] = 2.0f * (xz - wy);
    matrix.m[2][1] = 2.0f * (yz + wx);
    matrix.m[2][2] = 1.0f - (2.0f * (xx + yy));

    return matrix;
}

/**
 * @brief Rotates one 3D vector by a precomputed rotation matrix.
 * @param matrix Rotation matrix built by TrailGui_QuaternionToRotationMatrix;
 *               NULL is not allowed.
 * @param vector Vector to rotate.
 * @return Rotated vector.
 */
static TrailGui_Vector3 TrailGui_Mat3RotateVector(const TrailGui_Matrix3* matrix, TrailGui_Vector3 vector)
{
    TrailGui_Vector3 rotated;

    rotated.x = (matrix->m[0][0] * vector.x) + (matrix->m[0][1] * vector.y) + (matrix->m[0][2] * vector.z);
    rotated.y = (matrix->m[1][0] * vector.x) + (matrix->m[1][1] * vector.y) + (matrix->m[1][2] * vector.z);
    rotated.z = (matrix->m[2][0] * vector.x) + (matrix->m[2][1] * vector.y) + (matrix->m[2][2] * vector.z);

    return rotated;
}

/* Phone widgets -------------------------------------------------------------*/

/**
 * @brief Builds the phone orientation quaternion from one HM-10 packet.
 * @param hm10_packet Parsed phone packet. NULL is not allowed.
 * @return Normalized phone quaternion, or identity when the packet values are
 *         unusable.
 */
static TrailGui_Quaternion TrailGui_BuildPhoneQuaternion(const HM10_DataPacket* hm10_packet)
{
    TrailGui_Quaternion quaternion;

    quaternion.w = (float)hm10_packet->qw;
    quaternion.x = (float)hm10_packet->qx;
    quaternion.y = (float)hm10_packet->qy;
    quaternion.z = (float)hm10_packet->qz;

    return TrailGui_QuaternionNormalize(quaternion);
}

/**
 * @brief Draws a line-only 3D cuboid representing the phone orientation.
 * @param hm10_packet Parsed phone data packet. NULL is not allowed. The phone
 *                    quaternion fields are the sole orientation source; no
 *                    MPU-6050 data is used.
 * @param bounding_box LCD region that contains the complete phone render.
 *                     Reversed bounds are normalized internally, and out-of-
 *                     screen bounds are clipped. The cuboid rotates around the
 *                     center of this region and stays inside it.
 * @param line_width Cuboid edge thickness in pixels. A value of 0 leaves the
 *                   screen unchanged.
 * @param color ARGB8888 LCD color value used for all cuboid edges.
 * @return None.
 */
void TrailGui_RenderPhoneCuboid(const HM10_DataPacket* hm10_packet,
                              TrailGui_BoundingBox bounding_box,
                              uint16_t line_width,
                              uint32_t color)
{
    static const TrailGui_Vector3 model_vertices[8] = {
        {-TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, -TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, -TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, -TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, -TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, -TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {-TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, -TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {-TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, -TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, -TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, TRAIL_GUI_PHONE_MODEL_HALF_DEPTH},
        {-TRAIL_GUI_PHONE_MODEL_HALF_WIDTH, TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT, TRAIL_GUI_PHONE_MODEL_HALF_DEPTH}
    };
    static const uint8_t model_edges[12][2] = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
        {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
        {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
    };
    TrailGui_Point projected_points[8];
    TrailGui_Quaternion phone_quaternion;
    TrailGui_Matrix3 render_matrix;
    uint16_t width_px;
    uint16_t height_px;
    uint16_t min_dimension_px;
    float center_x;
    float center_y;
    float model_radius;
    float pixel_radius;
    float scale;
    uint8_t vertex_index;
    uint8_t edge_index;

    if ((hm10_packet == 0) || (line_width == 0U))
    {
        return;
    }

    if (TrailGui_NormalizeAndClipBoundingBox(&bounding_box) == 0U)
    {
        return;
    }

    width_px = TrailGui_GetBoundingBoxWidth(&bounding_box);
    height_px = TrailGui_GetBoundingBoxHeight(&bounding_box);
    min_dimension_px = TrailGui_MinUint16(width_px, height_px);

    if (min_dimension_px <= (uint16_t)((line_width * 2U) + 4U))
    {
        return;
    }

    static const float model_radius_const =
        TRAIL_GUI_PHONE_MODEL_HALF_WIDTH * TRAIL_GUI_PHONE_MODEL_HALF_WIDTH
        + TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT * TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT
        + TRAIL_GUI_PHONE_MODEL_HALF_DEPTH * TRAIL_GUI_PHONE_MODEL_HALF_DEPTH;
    model_radius = sqrtf(model_radius_const);
    pixel_radius = ((float)min_dimension_px * TRAIL_GUI_PHONE_MODEL_RADIUS_RATIO) - (float)line_width - 2.0f;

    if ((model_radius <= TRAIL_GUI_QUATERNION_EPSILON) || (pixel_radius <= 0.0f))
    {
        return;
    }

    scale = pixel_radius / model_radius;
    center_x = (float)bounding_box.x_min + (((float)width_px - 1.0f) * 0.5f);
    center_y = (float)bounding_box.y_min + (((float)height_px - 1.0f) * 0.5f);

    phone_quaternion = TrailGui_BuildPhoneQuaternion(hm10_packet);
    render_matrix = TrailGui_QuaternionToRotationMatrix(phone_quaternion);

    for (vertex_index = 0U; vertex_index < 8U; vertex_index++)
    {
        TrailGui_Vector3 rotated = TrailGui_Mat3RotateVector(&render_matrix, model_vertices[vertex_index]);
        int32_t screen_x = TrailGui_RoundFloatToInt32(center_x + (rotated.x * scale));
        int32_t screen_y = TrailGui_RoundFloatToInt32(center_y - (rotated.y * scale));

        projected_points[vertex_index].x = TrailGui_ClampInt32ToUint16(screen_x,
                                                                       bounding_box.x_min,
                                                                       bounding_box.x_max);
        projected_points[vertex_index].y = TrailGui_ClampInt32ToUint16(screen_y,
                                                                       bounding_box.y_min,
                                                                       bounding_box.y_max);
    }

    for (edge_index = 0U; edge_index < 12U; edge_index++)
    {
        TrailGui_DrawLine(projected_points[model_edges[edge_index][0]],
                          projected_points[model_edges[edge_index][1]],
                          line_width,
                          color);
    }
}

/**
 * @brief Draws formatted phone latitude/longitude text inside a bounding box.
 * @param hm10_packet Parsed phone data packet; NULL is not allowed.
 * @param bounding_box LCD region for the text. Reversed bounds are normalized
 *                     internally; out-of-screen bounds are clipped.
 * @param color ARGB8888 LCD color value used for the text.
 * @return None.
 */
void TrailGui_RenderPhoneGps(const HM10_DataPacket* hm10_packet,
                             TrailGui_BoundingBox bounding_box,
                             uint32_t color)
{
    char value[20];
    char text[32];
    uint16_t y_mid;
    uint16_t x_mid;

    if ((hm10_packet == NULL) || (TrailGui_NormalizeAndClipBoundingBox(&bounding_box) == 0U))
    {
        return;
    }

    y_mid = (bounding_box.y_min + bounding_box.y_max) / 2U - 5U;
    x_mid = (bounding_box.x_max - bounding_box.x_min) / 2U;

    UTIL_LCD_SetFont(&Font12);
    UTIL_LCD_SetTextColor(color);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);

    DebugTerminal_FormatFixed(value, sizeof(value), hm10_packet->lat_deg, 6U, 10U);
    snprintf(text, sizeof(text), "LAT:%s", value);
    UTIL_LCD_DisplayStringAt(bounding_box.x_min, y_mid, (uint8_t *)text, LEFT_MODE);

    DebugTerminal_FormatFixed(value, sizeof(value), hm10_packet->lon_deg, 6U, 10U);
    snprintf(text, sizeof(text), "LON:%s", value);
    UTIL_LCD_DisplayStringAt(bounding_box.x_min + x_mid, y_mid, (uint8_t *)text, LEFT_MODE);
}

/* Tilt indicator widget -----------------------------------------------------*/

/**
 * @brief Calculates the gravity-referenced tilt angle of one MPU-6050 sample.
 * @param mpu6050_packet Parsed MPU-6050 data packet; NULL is not allowed. Only
 *                       the scaled accelerometer fields are read, so the result
 *                       is an absolute angle instead of an integrated gyroscope
 *                       rate that would slowly drift away from level.
 * @return Tilt angle in degrees in the range [-180, 180]. The value is the
 *         rotation around the sensor Z axis, matching an MPU-6050 mounted
 *         upright so that its Z axis is horizontal and gravity therefore stays
 *         in the X-Y plane. It reads 0 with the sensor Y axis pointing up and
 *         90 with the sensor X axis pointing down, so a positive angle means
 *         the sensor X axis has tilted down. The rotation is mirrored against
 *         the raw sensor reading to match the installed orientation of the
 *         module.
 */
float TrailGui_TiltAngleFromAccelerometer(const MPU6050_DataPacket* mpu6050_packet)
{
    /*
     * Two mounting corrections ride along in this one call. Passing X before Y
     * carries the quarter turn between the sensor axes and the upright
     * position: rotating the pair a quarter turn adds 90 degrees to the angle,
     * so the reading lands on the Y-axis-up reference without a separate
     * addition, and it stays inside the [-180, 180] range atan2f already
     * returns instead of having to be wrapped back afterwards. Negating X then
     * mirrors the rotation, because the module faces the opposite way and
     * would otherwise lean the indicator against the physical tilt. Both leave
     * the level reading at 0, so neither disturbs the other.
     */
    return atan2f(-mpu6050_packet->accel_x_g, mpu6050_packet->accel_y_g) * TRAIL_GUI_RAD_TO_DEG;
}

/**
 * @brief Draws the numeric tilt readout followed by its degree ring.
 * @param bounding_box Normalized widget bounds in LCD pixels. The readout block
 *                     is centered horizontally inside these bounds and clamped
 *                     to them when the formatted text is wider than the region.
 * @param top_y Top row of the readout text in LCD pixel coordinates.
 * @param tilt_deg Tilt angle in degrees to print.
 * @param line_width Degree ring thickness in pixels. Values at or above
 *                   TRAIL_GUI_TILT_RING_RADIUS_PX draw the ring as a solid dot.
 * @param color ARGB8888 LCD color value used for the glyphs and the ring.
 * @param background_color ARGB8888 LCD color value already filling the region,
 *                         used behind the glyphs and inside the degree ring.
 * @return None.
 */
static void TrailGui_DrawTiltReadout(TrailGui_BoundingBox bounding_box,
                                     uint16_t top_y,
                                     float tilt_deg,
                                     uint16_t line_width,
                                     uint32_t color,
                                     uint32_t background_color)
{
    char text[12];
    uint16_t text_width_px;
    uint16_t block_width_px;
    uint16_t text_x;
    uint16_t ring_center_x;
    uint16_t ring_center_y;

    /*
     * The bundled fonts only cover ASCII 0x20 to 0x7E, so the degree sign is
     * drawn as a ring instead of printed. It is an outer disc punched out by a
     * background-colored inner disc, which keeps the ring stroke matched to the
     * indicator line without relying on stacked one-pixel circle outlines.
     */
    DebugTerminal_FormatFixed(text, sizeof(text), (double)tilt_deg, TRAIL_GUI_TILT_READOUT_DECIMALS, 0U);

    text_width_px = (uint16_t)(strlen(text) * Font24.Width);
    block_width_px = (uint16_t)(text_width_px + TRAIL_GUI_TILT_RING_GAP_PX +
        (2U * TRAIL_GUI_TILT_RING_RADIUS_PX) + 1U);
    text_x = TrailGui_ClampInt32ToUint16((int32_t)bounding_box.x_min +
                                             (((int32_t)TrailGui_GetBoundingBoxWidth(&bounding_box) -
                                                 (int32_t)block_width_px) / 2),
                                         bounding_box.x_min,
                                         bounding_box.x_max);
    ring_center_x = (uint16_t)(text_x + text_width_px + TRAIL_GUI_TILT_RING_GAP_PX +
        TRAIL_GUI_TILT_RING_RADIUS_PX);
    ring_center_y = (uint16_t)(top_y + (Font24.Height / 4U));

    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(color);
    UTIL_LCD_SetBackColor(background_color);
    UTIL_LCD_DisplayStringAt(text_x, top_y, (uint8_t*)text, LEFT_MODE);

    UTIL_LCD_FillCircle(ring_center_x, ring_center_y, TRAIL_GUI_TILT_RING_RADIUS_PX, color);

    if (line_width < TRAIL_GUI_TILT_RING_RADIUS_PX)
    {
        UTIL_LCD_FillCircle(ring_center_x,
                            ring_center_y,
                            (uint32_t)(TRAIL_GUI_TILT_RING_RADIUS_PX - line_width),
                            background_color);
    }
}

/**
 * @brief Draws the tilt indicator widget for one MPU-6050 sample.
 * @param mpu6050_packet Parsed MPU-6050 data packet. NULL is not allowed. The
 *                       scaled accelerometer fields are the sole tilt source, so
 *                       the reading is gravity referenced and does not drift the
 *                       way an integrated gyroscope rate would.
 * @param bounding_box LCD region that contains the complete widget. Reversed
 *                     bounds are normalized internally, and out-of-screen bounds
 *                     are clipped. The widget is centered inside this region and
 *                     stays inside it. Regions too small to hold the widget are
 *                     left unchanged.
 * @param line_width Indicator line thickness in pixels. A value of 0 leaves the
 *                   screen unchanged.
 * @param color ARGB8888 LCD color value used for every widget line and glyph.
 * @param background_color ARGB8888 LCD color value that already fills the
 *                         bounding box. Used behind the readout glyphs and
 *                         inside the degree ring; the widget never clears the
 *                         region itself, so the caller stays in charge of
 *                         erasing the previous frame.
 * @return None.
 */
void TrailGui_RenderTiltIndicator(const MPU6050_DataPacket* mpu6050_packet,
                                  TrailGui_BoundingBox bounding_box,
                                  uint16_t line_width,
                                  uint32_t color,
                                  uint32_t background_color)
{
    TrailGui_Point indicator_start;
    TrailGui_Point indicator_end;
    uint16_t width_px;
    uint16_t height_px;
    uint16_t min_dimension_px;
    uint16_t dial_radius_px;
    uint16_t datum_length_px;
    uint16_t datum_gap_px;
    uint16_t datum_extent_px;
    uint16_t widget_height_px;
    uint16_t center_x;
    uint16_t pivot_y;
    uint16_t readout_y;
    float tilt_deg;
    float tilt_rad;
    float offset_x;
    float offset_y;

    if ((mpu6050_packet == NULL) || (line_width == 0U))
    {
        return;
    }

    if (TrailGui_NormalizeAndClipBoundingBox(&bounding_box) == 0U)
    {
        return;
    }

    width_px = TrailGui_GetBoundingBoxWidth(&bounding_box);
    height_px = TrailGui_GetBoundingBoxHeight(&bounding_box);
    min_dimension_px = TrailGui_MinUint16(width_px, height_px);

    dial_radius_px = (uint16_t)((float)min_dimension_px * TRAIL_GUI_TILT_DIAL_RADIUS_RATIO);
    datum_length_px = (uint16_t)((float)min_dimension_px * TRAIL_GUI_TILT_DATUM_LENGTH_RATIO);
    datum_gap_px = (uint16_t)((float)min_dimension_px * TRAIL_GUI_TILT_DATUM_GAP_RATIO);
    datum_extent_px = (uint16_t)(dial_radius_px + datum_gap_px + datum_length_px);
    widget_height_px = (uint16_t)((2U * dial_radius_px) + TRAIL_GUI_TILT_READOUT_GAP_PX + Font24.Height);

    /*
     * The widget is one vertically centered block: the rotating dial on top and
     * the numeric readout underneath it. Rejecting regions that cannot hold the
     * block keeps every coordinate derived below inside the bounding box, so no
     * individual drawing step needs its own clipping pass.
     */
    if ((dial_radius_px == 0U) ||
        (widget_height_px >= height_px) ||
        (((2U * datum_extent_px) + 1U) > width_px))
    {
        return;
    }

    center_x = (uint16_t)(bounding_box.x_min + ((width_px - 1U) / 2U));
    pivot_y = (uint16_t)(bounding_box.y_min + ((height_px - widget_height_px) / 2U) + dial_radius_px);
    readout_y = (uint16_t)(pivot_y + dial_radius_px + TRAIL_GUI_TILT_READOUT_GAP_PX);

    /*
     * Quantizing to the printed resolution before any rendering keeps the drawn
     * line and the printed number describing the same angle, and it also drops
     * the "-0.0" that a tiny negative reading would otherwise format into.
     */
    tilt_deg = TrailGui_TiltAngleFromAccelerometer(mpu6050_packet);
    tilt_deg = (float)TrailGui_RoundFloatToInt32(tilt_deg * 10.0f) / 10.0f;
    tilt_rad = tilt_deg * TRAIL_GUI_DEG_TO_RAD;
    offset_x = cosf(tilt_rad) * (float)dial_radius_px;
    offset_y = sinf(tilt_rad) * (float)dial_radius_px;

    indicator_start.x = TrailGui_ClampInt32ToUint16(TrailGui_RoundFloatToInt32((float)center_x - offset_x),
                                                    bounding_box.x_min,
                                                    bounding_box.x_max);
    indicator_start.y = TrailGui_ClampInt32ToUint16(TrailGui_RoundFloatToInt32((float)pivot_y - offset_y),
                                                    bounding_box.y_min,
                                                    bounding_box.y_max);
    indicator_end.x = TrailGui_ClampInt32ToUint16(TrailGui_RoundFloatToInt32((float)center_x + offset_x),
                                                  bounding_box.x_min,
                                                  bounding_box.x_max);
    indicator_end.y = TrailGui_ClampInt32ToUint16(TrailGui_RoundFloatToInt32((float)pivot_y + offset_y),
                                                  bounding_box.y_min,
                                                  bounding_box.y_max);

    /* Level datum, kept outside the dial so it never touches the tilted line. */
    TrailGui_DrawLine((TrailGui_Point) {(uint16_t)(center_x - datum_extent_px), pivot_y},
                      (TrailGui_Point) {(uint16_t)(center_x - dial_radius_px - datum_gap_px), pivot_y},
                      line_width,
                      color);
    TrailGui_DrawLine((TrailGui_Point) {(uint16_t)(center_x + dial_radius_px + datum_gap_px), pivot_y},
                      (TrailGui_Point) {(uint16_t)(center_x + datum_extent_px), pivot_y},
                      line_width,
                      color);

    TrailGui_DrawLine(indicator_start, indicator_end, line_width, color);
    UTIL_LCD_FillCircle(center_x, pivot_y, TRAIL_GUI_TILT_PIVOT_RADIUS_PX, color);

    TrailGui_DrawTiltReadout(bounding_box, readout_y, tilt_deg, line_width, color, background_color);
}

/* Screens -------------------------------------------------------------------*/

/**
 * @brief Clears the full LCD screen with one solid color.
 * @param color ARGB8888 LCD color value passed directly to the LCD utility
 *              driver.
 * @return None.
 */
void TrailGui_ClearScreen(uint32_t color)
{
    UTIL_LCD_Clear(color);
}

/**
 * @brief Draws the initialization screen with an empty loading bar.
 * @param total_stage_count Number of equal loading stages in the full
 *                          initialization sequence. A value of 0 draws only
 *                          the empty bar frame.
 * @return None.
 */
void TrailGui_DrawLoadingScreen(uint16_t total_stage_count)
{
    TrailGui_ClearScreen(UTIL_LCD_COLOR_BLACK);

    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_DisplayStringAt(TRAIL_GUI_LOADING_TITLE_X,
                             TRAIL_GUI_LOADING_TITLE_Y,
                             (uint8_t*)TRAIL_GUI_TITLE,
                             LEFT_MODE);

    TrailGui_ExpandLoadingBar(0U, total_stage_count);
}

/**
 * @brief Renders the loading bar filled to the requested completed stage.
 * @param completed_stage_count Number of completed initialization stages. Values
 *                              greater than total_stage_count are clamped.
 * @param total_stage_count Total number of equal loading stages in the full
 *                          initialization sequence. A value of 0 draws an
 *                          empty bar.
 * @return None.
 */
void TrailGui_ExpandLoadingBar(uint16_t completed_stage_count, uint16_t total_stage_count)
{
    uint32_t inner_x;
    uint32_t inner_y;
    uint32_t inner_width;
    uint32_t inner_height;
    uint32_t fill_width = 0U;

    if (completed_stage_count > total_stage_count)
    {
        completed_stage_count = total_stage_count;
    }

    UTIL_LCD_FillRect(TRAIL_GUI_LOADING_BAR_X,
                      TRAIL_GUI_LOADING_BAR_Y,
                      TRAIL_GUI_LOADING_BAR_WIDTH,
                      TRAIL_GUI_LOADING_BAR_HEIGHT,
                      UTIL_LCD_COLOR_BLACK);

    UTIL_LCD_FillRect(TRAIL_GUI_LOADING_BAR_X,
                      TRAIL_GUI_LOADING_BAR_Y,
                      TRAIL_GUI_LOADING_BAR_WIDTH,
                      TRAIL_GUI_LOADING_BAR_BORDER_WIDTH,
                      UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_FillRect(TRAIL_GUI_LOADING_BAR_X,
                      (uint32_t)(TRAIL_GUI_LOADING_BAR_Y + TRAIL_GUI_LOADING_BAR_HEIGHT -
                          TRAIL_GUI_LOADING_BAR_BORDER_WIDTH),
                      TRAIL_GUI_LOADING_BAR_WIDTH,
                      TRAIL_GUI_LOADING_BAR_BORDER_WIDTH,
                      UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_FillRect(TRAIL_GUI_LOADING_BAR_X,
                      TRAIL_GUI_LOADING_BAR_Y,
                      TRAIL_GUI_LOADING_BAR_BORDER_WIDTH,
                      TRAIL_GUI_LOADING_BAR_HEIGHT,
                      UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_FillRect(
        (uint32_t)(TRAIL_GUI_LOADING_BAR_X + TRAIL_GUI_LOADING_BAR_WIDTH - TRAIL_GUI_LOADING_BAR_BORDER_WIDTH),
        TRAIL_GUI_LOADING_BAR_Y,
        TRAIL_GUI_LOADING_BAR_BORDER_WIDTH,
        TRAIL_GUI_LOADING_BAR_HEIGHT,
        UTIL_LCD_COLOR_WHITE);

    inner_x = TRAIL_GUI_LOADING_BAR_X + TRAIL_GUI_LOADING_BAR_BORDER_WIDTH + TRAIL_GUI_LOADING_BAR_PADDING;
    inner_y = TRAIL_GUI_LOADING_BAR_Y + TRAIL_GUI_LOADING_BAR_BORDER_WIDTH + TRAIL_GUI_LOADING_BAR_PADDING;
    inner_width = TRAIL_GUI_LOADING_BAR_WIDTH - (2U * (TRAIL_GUI_LOADING_BAR_BORDER_WIDTH +
        TRAIL_GUI_LOADING_BAR_PADDING));
    inner_height = TRAIL_GUI_LOADING_BAR_HEIGHT - (2U * (TRAIL_GUI_LOADING_BAR_BORDER_WIDTH +
        TRAIL_GUI_LOADING_BAR_PADDING));

    if (total_stage_count > 0U)
    {
        fill_width = (inner_width * (uint32_t)completed_stage_count) / (uint32_t)total_stage_count;
    }

    if (fill_width > inner_width)
    {
        fill_width = inner_width;
    }

    if (fill_width > 0U)
    {
        UTIL_LCD_FillRect(inner_x, inner_y, fill_width, inner_height, UTIL_LCD_COLOR_WHITE);
    }
}

/**
 * @brief Draws the default trail-hud LCD layout.
 * @param None.
 * @return None.
 */
void TrailGui_DrawDefaultScreen(void)
{
    TrailGui_BoundingBox olive_background = {
        .x_min = 0U,
        .x_max = TRAIL_GUI_SCREEN_WIDTH - 1U,
        .y_min = 0U,
        .y_max = TRAIL_GUI_SCREEN_HEIGHT - 1U
    };

    TrailGui_BoundingBox gyroscope_background = {
        .x_min = TRAIL_GUI_GYROSCOPE_BACKGROUND_X_MIN,
        .x_max = TRAIL_GUI_GYROSCOPE_BACKGROUND_X_MAX,
        .y_min = TRAIL_GUI_GYROSCOPE_BACKGROUND_Y_MIN,
        .y_max = TRAIL_GUI_GYROSCOPE_BACKGROUND_Y_MAX
    };

    TrailGui_ClearScreen(UTIL_LCD_COLOR_BLACK);
    TrailGui_DrawRoundedRectangle(olive_background, 5U, TRAIL_GUI_COLOR_LIGHT_OLIVE);
    TrailGui_DrawRoundedRectangle(gyroscope_background, 5U, UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_SetBackColor(TRAIL_GUI_COLOR_LIGHT_OLIVE);
    UTIL_LCD_DisplayStringAt(12U, 12U, (uint8_t*)TRAIL_GUI_TITLE, LEFT_MODE);
    TrailGui_DrawLine((TrailGui_Point) {8U, 37U}, (TrailGui_Point) {165U, 37U}, 2U, UTIL_LCD_COLOR_BLACK);
}
