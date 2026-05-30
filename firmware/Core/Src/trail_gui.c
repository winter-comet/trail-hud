#include "trail_gui.h"
#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"
#include <math.h>

#define TRAIL_GUI_TITLE "TRAIL-HUD"
#define TRAIL_GUI_SPLIT_LINE_X ((TRAIL_GUI_SCREEN_WIDTH - TRAIL_GUI_LINE_WIDTH_THIN) / 2U)
#define TRAIL_GUI_PHONE_MODEL_HALF_WIDTH 0.38f
#define TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT 0.80f
#define TRAIL_GUI_PHONE_MODEL_HALF_DEPTH 0.055f
#define TRAIL_GUI_PHONE_MODEL_RADIUS_RATIO 0.38f
#define TRAIL_GUI_GYRO_OFFSET_SECONDS 0.02f
#define TRAIL_GUI_DEG_TO_RAD 0.01745329251994329577f
#define TRAIL_GUI_QUATERNION_EPSILON 0.000001f

typedef struct
{
    float x;
    float y;
    float z;
} TrailGui_Vector3;

typedef struct
{
    float w;
    float x;
    float y;
    float z;
} TrailGui_Quaternion;

static uint8_t TrailGui_NormalizeAndClipBoundingBox(TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxWidth(const TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxHeight(const TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px, uint16_t width_px, uint16_t height_px);
static uint16_t TrailGui_ClampRadius(uint16_t radius_px, uint16_t width_px, uint16_t height_px);
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px, uint16_t distance_from_corner_center_px);
static int32_t TrailGui_Abs32(int32_t value);
static void TrailGui_FillLinePoint(int32_t center_x, int32_t center_y, uint16_t width, uint32_t color);
static uint16_t TrailGui_MinUint16(uint16_t lhs, uint16_t rhs);
static int32_t TrailGui_RoundFloatToInt32(float value);
static uint16_t TrailGui_ClampInt32ToUint16(int32_t value, uint16_t min_value, uint16_t max_value);
static TrailGui_Quaternion TrailGui_QuaternionIdentity(void);
static TrailGui_Quaternion TrailGui_QuaternionNormalize(TrailGui_Quaternion quaternion);
static TrailGui_Quaternion TrailGui_QuaternionConjugate(TrailGui_Quaternion quaternion);
static TrailGui_Quaternion TrailGui_QuaternionMultiply(TrailGui_Quaternion lhs, TrailGui_Quaternion rhs);
static TrailGui_Vector3 TrailGui_QuaternionRotateVector(TrailGui_Quaternion quaternion, TrailGui_Vector3 vector);
static TrailGui_Quaternion TrailGui_BuildPhoneQuaternion(const HM10_DataPacket* hm10_packet);
static TrailGui_Quaternion TrailGui_BuildVectorToVectorQuaternion(TrailGui_Vector3 from, TrailGui_Vector3 to);
static TrailGui_Quaternion TrailGui_BuildMpuGravityQuaternion(const MPU6050_DataPacket* mpu6050_packet);
static TrailGui_Quaternion TrailGui_BuildMpuGyroQuaternion(const MPU6050_DataPacket* mpu6050_packet);
static TrailGui_Quaternion TrailGui_BuildMpuOffsetQuaternion(const MPU6050_DataPacket* mpu6050_packet);

/**
 * @brief Normalizes and clips a bounding box to the LCD screen area.
 * @param bounding_box Pointer to the bounding box to normalize and clip. The
 *                     pointer must not be NULL. Reversed min/max coordinates
 *                     are swapped internally, and coordinates beyond the right
 *                     or bottom screen edge are clipped.
 * @return 1 if the resulting bounding box is valid and visible on the screen,
 *         0 if the input pointer is NULL or the box is fully outside the
 *         drawable LCD area.
 */
static uint8_t TrailGui_NormalizeAndClipBoundingBox(TrailGui_BoundingBox* bounding_box)
{
    uint16_t tmp;

    if (bounding_box == 0)
    {
        return 0U;
    }

    if (bounding_box->x_min > bounding_box->x_max)
    {
        tmp = bounding_box->x_min;
        bounding_box->x_min = bounding_box->x_max;
        bounding_box->x_max = tmp;
    }

    if (bounding_box->y_min > bounding_box->y_max)
    {
        tmp = bounding_box->y_min;
        bounding_box->y_min = bounding_box->y_max;
        bounding_box->y_max = tmp;
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

    if ((bounding_box->x_max < bounding_box->x_min) ||
        (bounding_box->y_max < bounding_box->y_min))
    {
        return 0U;
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

/**
 * @brief Clamps a corner edge length so opposite corners do not overlap.
 * @param corner_length_px Requested corner edge length in pixels.
 * @param width_px Width of the target bounding box in pixels.
 * @param height_px Height of the target bounding box in pixels.
 * @return Clamped corner edge length in pixels.
 */
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px,
                                           uint16_t width_px,
                                           uint16_t height_px)
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
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px,
                                           uint16_t distance_from_corner_center_px)
{
    uint32_t radius_squared;
    uint32_t distance_squared;
    uint32_t x_extent;

    if (radius_px == 0U)
    {
        return 0U;
    }

    radius_squared = (uint32_t)radius_px * (uint32_t)radius_px;
    distance_squared = (uint32_t)distance_from_corner_center_px *
        (uint32_t)distance_from_corner_center_px;
    x_extent = radius_px;

    while ((x_extent > 0U) && (((x_extent * x_extent) + distance_squared) > radius_squared))
    {
        x_extent--;
    }

    return (uint16_t)((uint32_t)radius_px - x_extent);
}

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
 * @brief Draws the TRAIL-MODULE title text box.
 * @param None.
 * @return None.
 */
void TrailGui_DrawTitleText(void)
{
    TrailGui_BoundingBox rectangle_box = {
        .x_min = 6U,
        .x_max = 233U,
        .y_min = 6U,
        .y_max = 38U
    };

    TrailGui_DrawRoundedRectangle(rectangle_box, 5U, UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_DisplayStringAt(12U, 12U, (uint8_t*)TRAIL_GUI_TITLE, LEFT_MODE);
}

/**
 * @brief Draws a vertical divider through the center of the screen.
 * @param None.
 * @return None.
 */
void TrailGui_DrawVerticalSplitLine(void)
{
    UTIL_LCD_FillRect(TRAIL_GUI_SPLIT_LINE_X,
                      0U,
                      TRAIL_GUI_LINE_WIDTH_THIN,
                      TRAIL_GUI_SCREEN_HEIGHT,
                      UTIL_LCD_COLOR_BLACK);
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
void TrailGui_DrawRoundedRectangle(TrailGui_BoundingBox bounding_box,
                                   uint16_t radius_px,
                                   uint32_t color)
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
        UTIL_LCD_FillRect(bounding_box.x_min,
                          bounding_box.y_min,
                          width_px,
                          height_px,
                          color);
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
static void TrailGui_FillLinePoint(int32_t center_x,
                                   int32_t center_y,
                                   uint16_t width,
                                   uint32_t color)
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

    UTIL_LCD_FillRect((uint32_t)x,
                      (uint32_t)y,
                      (uint32_t)rect_width,
                      (uint32_t)rect_height,
                      color);
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
void TrailGui_DrawLine(TrailGui_Point start,
                       TrailGui_Point end,
                       uint16_t width,
                       uint32_t color)
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
static uint16_t TrailGui_ClampInt32ToUint16(int32_t value,
                                            uint16_t min_value,
                                            uint16_t max_value)
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
 * @brief Returns the conjugate of a unit quaternion.
 * @param quaternion Quaternion to conjugate.
 * @return Quaternion conjugate.
 */
static TrailGui_Quaternion TrailGui_QuaternionConjugate(TrailGui_Quaternion quaternion)
{
    quaternion.x = -quaternion.x;
    quaternion.y = -quaternion.y;
    quaternion.z = -quaternion.z;
    return quaternion;
}

/**
 * @brief Multiplies two quaternions.
 * @param lhs Left-hand quaternion.
 * @param rhs Right-hand quaternion.
 * @return Product quaternion representing lhs followed by rhs under the
 *         library's convention.
 */
static TrailGui_Quaternion TrailGui_QuaternionMultiply(TrailGui_Quaternion lhs,
                                                       TrailGui_Quaternion rhs)
{
    TrailGui_Quaternion product;

    product.w = (lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z);
    product.x = (lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y);
    product.y = (lhs.w * rhs.y) - (lhs.x * rhs.z) + (lhs.y * rhs.w) + (lhs.z * rhs.x);
    product.z = (lhs.w * rhs.z) + (lhs.x * rhs.y) - (lhs.y * rhs.x) + (lhs.z * rhs.w);

    return product;
}

/**
 * @brief Rotates one 3D vector by a quaternion.
 * @param quaternion Unit quaternion used as the rotation.
 * @param vector Vector to rotate.
 * @return Rotated vector.
 */
static TrailGui_Vector3 TrailGui_QuaternionRotateVector(TrailGui_Quaternion quaternion,
                                                        TrailGui_Vector3 vector)
{
    TrailGui_Quaternion vector_quaternion = {0.0f, vector.x, vector.y, vector.z};
    TrailGui_Quaternion rotated_quaternion;
    TrailGui_Vector3 rotated_vector;

    rotated_quaternion = TrailGui_QuaternionMultiply(
        TrailGui_QuaternionMultiply(quaternion, vector_quaternion),
        TrailGui_QuaternionConjugate(quaternion));

    rotated_vector.x = rotated_quaternion.x;
    rotated_vector.y = rotated_quaternion.y;
    rotated_vector.z = rotated_quaternion.z;

    return rotated_vector;
}

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
 * @brief Builds a quaternion that rotates one unit vector into another.
 * @param from Source unit vector.
 * @param to Destination unit vector.
 * @return Quaternion that maps from to to, or a stable 180-degree fallback when
 *         the vectors point in opposite directions.
 */
static TrailGui_Quaternion TrailGui_BuildVectorToVectorQuaternion(TrailGui_Vector3 from,
                                                                  TrailGui_Vector3 to)
{
    TrailGui_Quaternion quaternion;
    float dot = (from.x * to.x) + (from.y * to.y) + (from.z * to.z);

    if (dot < -0.9999f)
    {
        quaternion.w = 0.0f;
        quaternion.x = 1.0f;
        quaternion.y = 0.0f;
        quaternion.z = 0.0f;
        return quaternion;
    }

    quaternion.w = 1.0f + dot;
    quaternion.x = (from.y * to.z) - (from.z * to.y);
    quaternion.y = (from.z * to.x) - (from.x * to.z);
    quaternion.z = (from.x * to.y) - (from.y * to.x);

    return TrailGui_QuaternionNormalize(quaternion);
}

/**
 * @brief Builds a board tilt quaternion from the MPU-6050 acceleration vector.
 * @param mpu6050_packet Latest MPU-6050 packet. NULL is not allowed.
 * @return Quaternion that approximately maps the board gravity vector back to
 *         the default +Z gravity direction, or identity when acceleration is too
 *         small to use.
 */
static TrailGui_Quaternion TrailGui_BuildMpuGravityQuaternion(const MPU6050_DataPacket* mpu6050_packet)
{
    TrailGui_Vector3 measured_gravity;
    TrailGui_Vector3 reference_gravity = {0.0f, 0.0f, 1.0f};
    float length_squared;
    float inv_length;

    measured_gravity.x = mpu6050_packet->accel_x_g;
    measured_gravity.y = mpu6050_packet->accel_y_g;
    measured_gravity.z = mpu6050_packet->accel_z_g;

    length_squared = (measured_gravity.x * measured_gravity.x) +
        (measured_gravity.y * measured_gravity.y) +
        (measured_gravity.z * measured_gravity.z);

    if (length_squared <= TRAIL_GUI_QUATERNION_EPSILON)
    {
        return TrailGui_QuaternionIdentity();
    }

    inv_length = 1.0f / sqrtf(length_squared);
    measured_gravity.x *= inv_length;
    measured_gravity.y *= inv_length;
    measured_gravity.z *= inv_length;

    return TrailGui_BuildVectorToVectorQuaternion(measured_gravity, reference_gravity);
}

/**
 * @brief Builds a small rate-based MPU-6050 gyroscope offset quaternion.
 * @param mpu6050_packet Latest MPU-6050 packet. NULL is not allowed.
 * @return Small normalized quaternion derived from the current angular velocity.
 */
static TrailGui_Quaternion TrailGui_BuildMpuGyroQuaternion(const MPU6050_DataPacket* mpu6050_packet)
{
    TrailGui_Quaternion quaternion;
    float half_step = 0.5f * TRAIL_GUI_GYRO_OFFSET_SECONDS * TRAIL_GUI_DEG_TO_RAD;

    quaternion.w = 1.0f;
    quaternion.x = mpu6050_packet->gyro_x_dps * half_step;
    quaternion.y = mpu6050_packet->gyro_y_dps * half_step;
    quaternion.z = mpu6050_packet->gyro_z_dps * half_step;

    return TrailGui_QuaternionNormalize(quaternion);
}

/**
 * @brief Builds the approximate MPU-6050 board-offset quaternion.
 * @param mpu6050_packet Latest MPU-6050 packet. NULL is not allowed.
 * @return Normalized quaternion combining gravity tilt compensation with a
 *         small gyroscope-rate offset.
 */
static TrailGui_Quaternion TrailGui_BuildMpuOffsetQuaternion(const MPU6050_DataPacket* mpu6050_packet)
{
    TrailGui_Quaternion gravity_quaternion = TrailGui_BuildMpuGravityQuaternion(mpu6050_packet);
    TrailGui_Quaternion gyro_quaternion = TrailGui_BuildMpuGyroQuaternion(mpu6050_packet);

    return TrailGui_QuaternionNormalize(TrailGui_QuaternionMultiply(gyro_quaternion,
                                                                    gravity_quaternion));
}

/**
 * @brief Draws a line-only 3D cuboid representing the phone orientation.
 * @param hm10_packet Parsed phone data packet. NULL is not allowed. The phone
 *                    quaternion fields are used as the main orientation source.
 * @param mpu6050_packet Latest MPU-6050 data packet. NULL is not allowed. The
 *                       accelerometer values provide board tilt compensation,
 *                       while gyroscope values add a small rate-based visual
 *                       offset because the packet does not store an integrated
 *                       absolute MPU orientation.
 * @param bounding_box LCD region that contains the complete phone render.
 *                     Reversed bounds are normalized internally, and out-of-
 *                     screen bounds are clipped. The cuboid rotates around the
 *                     center of this region and stays inside it.
 * @param line_width Cuboid edge thickness in pixels. A value of 0 leaves the
 *                   screen unchanged.
 * @param color ARGB8888 LCD color value used for all cuboid edges.
 * @return None.
 */
void TrailGui_DrawPhoneCuboid(const HM10_DataPacket* hm10_packet,
                              const MPU6050_DataPacket* mpu6050_packet,
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
    TrailGui_Quaternion mpu_offset_quaternion;
    TrailGui_Quaternion render_quaternion;
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

    if ((hm10_packet == 0) || (mpu6050_packet == 0) || (line_width == 0U))
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

    model_radius = sqrtf((TRAIL_GUI_PHONE_MODEL_HALF_WIDTH * TRAIL_GUI_PHONE_MODEL_HALF_WIDTH) +
        (TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT * TRAIL_GUI_PHONE_MODEL_HALF_HEIGHT) +
        (TRAIL_GUI_PHONE_MODEL_HALF_DEPTH * TRAIL_GUI_PHONE_MODEL_HALF_DEPTH));
    pixel_radius = ((float)min_dimension_px * TRAIL_GUI_PHONE_MODEL_RADIUS_RATIO) -
        (float)line_width - 2.0f;

    if ((model_radius <= TRAIL_GUI_QUATERNION_EPSILON) || (pixel_radius <= 0.0f))
    {
        return;
    }

    scale = pixel_radius / model_radius;
    center_x = (float)bounding_box.x_min + (((float)width_px - 1.0f) * 0.5f);
    center_y = (float)bounding_box.y_min + (((float)height_px - 1.0f) * 0.5f);

    phone_quaternion = TrailGui_BuildPhoneQuaternion(hm10_packet);
    mpu_offset_quaternion = TrailGui_BuildMpuOffsetQuaternion(mpu6050_packet);
    render_quaternion = TrailGui_QuaternionNormalize(
        TrailGui_QuaternionMultiply(TrailGui_QuaternionConjugate(mpu_offset_quaternion),
                                    phone_quaternion));

    for (vertex_index = 0U; vertex_index < 8U; vertex_index++)
    {
        TrailGui_Vector3 rotated = TrailGui_QuaternionRotateVector(render_quaternion,
                                                                   model_vertices[vertex_index]);
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
 * @brief Draws the default trail-hud LCD layout.
 * @param None.
 * @return None.
 */
void TrailGui_DrawDefaultScreen(void)
{
    TrailGui_BoundingBox menu_widget_bounds = {
        .x_min = 0U,
        .x_max = 239U,
        .y_min = 0U,
        .y_max = 272U
    };
    TrailGui_BoundingBox path_widget_bounds = {
        .x_min = 6U,
        .x_max = 233U,
        .y_min = 44U,
        .y_max = 265U
    };
    TrailGui_Point start_point = {223U, 54U};
    TrailGui_Point end_point = {16U, 255U};

    TrailGui_ClearScreen(UTIL_LCD_COLOR_BLACK);
    TrailGui_DrawRoundedRectangle(menu_widget_bounds, 5U, TRAIL_GUI_COLOR_LIGHT_OLIVE);
    TrailGui_DrawRoundedRectangle(path_widget_bounds, 5U, UTIL_LCD_COLOR_WHITE);
    TrailGui_DrawTitleText();
    TrailGui_DrawLine(start_point, end_point, TRAIL_GUI_LINE_WIDTH_THIN, UTIL_LCD_COLOR_BLACK);
}
