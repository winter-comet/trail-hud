#ifndef TRAIL_GUI_H
#define TRAIL_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRAIL_GUI_SCREEN_WIDTH      480U
#define TRAIL_GUI_SCREEN_HEIGHT     272U
#define TRAIL_GUI_COLOR_LIGHT_OLIVE 0xFF607036UL
#define TRAIL_GUI_LINE_WIDTH_THIN   2U
#define TRAIL_GUI_LINE_WIDTH_THICK  (TRAIL_GUI_LINE_WIDTH_THIN * 2)
#define TRAIL_GUI_CORNER_COUNT      4U

typedef struct
{
    uint16_t x;
    uint16_t y;
} TrailGui_Point;

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
 * @brief Draws a corner-only bounding rectangle around four supplied points.
 * @param points Four points that define the object to bound. NULL is not allowed.
 *               The function computes an axis-aligned bounding box, so the points
 *               may be passed in any order.
 * @param corner_length_px Length of each visible corner edge in pixels. A value
 *                         of 0 leaves the screen unchanged.
 * @param color ARGB8888 LCD color value used for the corner lines.
 * @return None.
 */
void TrailGui_DrawCornerBoundingRectangle(const TrailGui_Point points[TRAIL_GUI_CORNER_COUNT],
                                          uint16_t corner_length_px,
                                          uint32_t color);

/**
 * @brief Draws a filled rounded rectangle inside the bounds defined by four points.
 * @param points Four points that define the rectangle bounds. NULL is not allowed.
 *               The function computes an axis-aligned bounding box, so the points
 *               may be passed in any order. The drawn shape always stays within
 *               this bounding box.
 * @param radius_px Corner radius in pixels. Values larger than half of the
 *                  rectangle width or height are clamped automatically.
 * @param color ARGB8888 LCD color value used to fill the rounded rectangle.
 * @return None.
 */
void TrailGui_FillRoundedRectangle(const TrailGui_Point points[TRAIL_GUI_CORNER_COUNT],
                                   uint16_t radius_px,
                                   uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* TRAIL_GUI_H */