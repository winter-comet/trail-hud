#ifndef TRAIL_GUI_H
#define TRAIL_GUI_H

#include <stdint.h>
#include "hm10_packet.h"
#include "mpu6050_packet.h"

#ifdef __cplusplus
extern "C" {


#endif

#define TRAIL_GUI_SCREEN_MARGIN 6U
#define TRAIL_GUI_LINE_WIDTH_THIN 2U
#define TRAIL_GUI_LINE_WIDTH_THICK (TRAIL_GUI_LINE_WIDTH_THIN * 2U)
#define TRAIL_GUI_COLOR_LIGHT_OLIVE 0xFF607036UL
#define TRAIL_GUI_SCREEN_WIDTH 480U
#define TRAIL_GUI_SCREEN_HEIGHT 272U

/**
 * @brief Stores one point in LCD pixel coordinates.
 *
 * Fields:
 * - x: Horizontal position in pixels, measured from the left edge of the LCD.
 * - y: Vertical position in pixels, measured from the top edge of the LCD.
 */
typedef struct
{
    uint16_t x;
    uint16_t y;
} TrailGui_Point;

/**
 * @brief Stores the inclusive pixel bounds of a rectangular LCD region.
 *
 * Fields:
 * - x_min: Left edge of the bounding box in LCD pixel coordinates.
 * - x_max: Right edge of the bounding box in LCD pixel coordinates.
 * - y_min: Top edge of the bounding box in LCD pixel coordinates.
 * - y_max: Bottom edge of the bounding box in LCD pixel coordinates.
 */
typedef struct
{
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
} TrailGui_BoundingBox;

/**
 * @brief Clears the full LCD screen with one solid color.
 * @param color ARGB8888 LCD color value passed directly to the LCD utility
 *              driver.
 * @return None.
 */
void TrailGui_ClearScreen(uint32_t color);

/**
 * @brief Draws the default trail-hud LCD layout.
 * @param None.
 * @return None.
 */
void TrailGui_DrawDefaultScreen(void);

/**
 * @brief Draws the TRAIL-MODULE title text box.
 * @param None.
 * @return None.
 */
void TrailGui_DrawTitleText(void);

/**
 * @brief Draws a vertical divider through the center of the screen.
 * @param None.
 * @return None.
 */
void TrailGui_DrawVerticalSplitLine(void);

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
                                    uint32_t color);

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
                                   uint32_t color);

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
                       uint32_t color);

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
                              uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* TRAIL_GUI_H */
