#ifndef TRAIL_GUI_H
#define TRAIL_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRAIL_GUI_SCREEN_MARGIN     6U
#define TRAIL_GUI_LINE_WIDTH_THIN   2U
#define TRAIL_GUI_LINE_WIDTH_THICK  (TRAIL_GUI_LINE_WIDTH_THIN * 2U)
#define TRAIL_GUI_COLOR_LIGHT_OLIVE 0xFF607036UL

#define TRAIL_GUI_SCREEN_WIDTH      480U
#define TRAIL_GUI_SCREEN_HEIGHT     272U

typedef struct
{
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
} TrailGui_BoundingBox;

/**
 * @brief Clears the full LCD screen with one solid color.
 * @param color ARGB8888 LCD color value.
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
 * @brief Draws the underlined TRAIL-MODULE title box.
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
 *                     normalized internally. Bounds outside the screen are clipped.
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
 *                     normalized internally. Bounds outside the screen are clipped.
 *                     The drawn shape always stays inside this bounding box.
 * @param radius_px Corner radius in pixels. Values larger than the useful half
 *                  extents of the rectangle are clamped automatically.
 * @param color ARGB8888 LCD color value used to fill the rounded rectangle.
 * @return None.
 */
void TrailGui_DrawRoundedRectangle(TrailGui_BoundingBox bounding_box,
                                   uint16_t radius_px,
                                   uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* TRAIL_GUI_H */
