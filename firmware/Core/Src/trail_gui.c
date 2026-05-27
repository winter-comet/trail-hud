#include "trail_gui.h"
#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"

#define TRAIL_GUI_TITLE                    "TRAIL-MODULE"
#define TRAIL_GUI_SPLIT_LINE_X             ((TRAIL_GUI_SCREEN_WIDTH - TRAIL_GUI_LINE_WIDTH_THIN) / 2U)

static uint8_t TrailGui_NormalizeAndClipBoundingBox(TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxWidth(const TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_GetBoundingBoxHeight(const TrailGui_BoundingBox* bounding_box);
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px,
                                           uint16_t width_px,
                                           uint16_t height_px);
static uint16_t TrailGui_ClampRadius(uint16_t radius_px,
                                     uint16_t width_px,
                                     uint16_t height_px);
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px,
                                           uint16_t distance_from_corner_center_px);
static int32_t TrailGui_Abs32(int32_t value);
static void TrailGui_FillLinePoint(int32_t center_x,
                                   int32_t center_y,
                                   uint16_t width,
                                   uint32_t color);

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

    if ((bounding_box->x_min >= TRAIL_GUI_SCREEN_WIDTH)
        || (bounding_box->y_min >= TRAIL_GUI_SCREEN_HEIGHT))
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

    if ((bounding_box->x_max < bounding_box->x_min)
        || (bounding_box->y_max < bounding_box->y_min))
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
static uint16_t TrailGui_ClampRadius(uint16_t radius_px,
                                     uint16_t width_px,
                                     uint16_t height_px)
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
    distance_squared = (uint32_t)distance_from_corner_center_px
        * (uint32_t)distance_from_corner_center_px;
    x_extent = radius_px;

    while ((x_extent > 0U)
        && (((x_extent * x_extent) + distance_squared) > radius_squared))
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

    UTIL_LCD_DisplayStringAt(12U,
                             12U,
                             (uint8_t*)TRAIL_GUI_TITLE,
                             LEFT_MODE);
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
 * @brief Draws the default trail-hud LCD layout.
 * @param None.
 * @return None.
 */
void TrailGui_DrawDefaultScreen(void)
{
    TrailGui_ClearScreen(UTIL_LCD_COLOR_BLACK);
    TrailGui_BoundingBox menu_widget_bounds = {
        .x_min = 0U,
        .x_max = 239U,
        .y_min = 0U,
        .y_max = 272U
    };
    TrailGui_DrawRoundedRectangle(menu_widget_bounds, 5U, TRAIL_GUI_COLOR_LIGHT_OLIVE);
    TrailGui_BoundingBox path_widget_bounds = {
        .x_min = 6U,
        .x_max = 233U,
        .y_min = 44U,
        .y_max = 265U
    };
    TrailGui_DrawRoundedRectangle(path_widget_bounds, 5U, UTIL_LCD_COLOR_WHITE);
    TrailGui_DrawTitleText();

    TrailGui_Point start_point = {223U, 54U};
    TrailGui_Point end_point = {16U, 255U};
    TrailGui_DrawLine(start_point, end_point, TRAIL_GUI_LINE_WIDTH_THIN, UTIL_LCD_COLOR_BLACK);
}
