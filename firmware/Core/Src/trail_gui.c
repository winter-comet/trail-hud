#include "trail_gui.h"
#include "stm32h750b_discovery_lcd.h"
#include "stm32_lcd.h"
#include <stddef.h>

#define TRAIL_GUI_TITLE                    "TRAIL-MODULE"
#define TRAIL_GUI_TITLE_X                  8U
#define TRAIL_GUI_TITLE_Y                  8U
#define TRAIL_GUI_TITLE_CHAR_COUNT         ((uint32_t)(sizeof(TRAIL_GUI_TITLE) - 1U))
#define TRAIL_GUI_TITLE_BOX_X              4U
#define TRAIL_GUI_TITLE_BOX_Y              4U
#define TRAIL_GUI_TITLE_BOX_WIDTH          215U
#define TRAIL_GUI_TITLE_BOX_HEIGHT         34U

#define TRAIL_GUI_SPLIT_LINE_X             ((TRAIL_GUI_SCREEN_WIDTH - TRAIL_GUI_LINE_WIDTH_THIN) / 2U)

static uint16_t TrailGui_MinU16(uint16_t a, uint16_t b);
static uint16_t TrailGui_MaxU16(uint16_t a, uint16_t b);
static uint16_t TrailGui_ClampCornerLength(uint16_t corner_length_px,
                                           uint16_t width_px,
                                           uint16_t height_px);
static uint16_t TrailGui_ClampToHalfExtents(uint16_t value_px,
                                            uint16_t width_px,
                                            uint16_t height_px);
static uint16_t TrailGui_CircleInsetForRow(uint16_t radius_px,
                                           uint16_t distance_from_corner_center_px);

static uint16_t TrailGui_MinU16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

static uint16_t TrailGui_MaxU16(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}

static uint16_t TrailGui_ClampToHalfExtents(uint16_t value_px,
                                            uint16_t width_px,
                                            uint16_t height_px)
{
    uint16_t max_value = width_px / 2U;

    if ((height_px / 2U) < max_value)
    {
        max_value = height_px / 2U;
    }

    if (value_px > max_value)
    {
        return max_value;
    }

    return value_px;
}

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

    while (((x_extent * x_extent) + distance_squared) > radius_squared)
    {
        x_extent--;
    }

    return (uint16_t)((uint32_t)radius_px - x_extent);
}

void TrailGui_ClearScreen(uint32_t color)
{
    UTIL_LCD_Clear(color);
}

void TrailGui_DrawTitleText(void)
{
    UTIL_LCD_FillRect(TRAIL_GUI_TITLE_BOX_X,
                      TRAIL_GUI_TITLE_BOX_Y,
                      TRAIL_GUI_TITLE_BOX_WIDTH,
                      TRAIL_GUI_TITLE_BOX_HEIGHT,
                      UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_DrawRect(TRAIL_GUI_TITLE_BOX_X,
                      TRAIL_GUI_TITLE_BOX_Y,
                      TRAIL_GUI_TITLE_BOX_WIDTH,
                      TRAIL_GUI_TITLE_BOX_HEIGHT,
                      UTIL_LCD_COLOR_BLACK);

    UTIL_LCD_SetFont(&Font24);
    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLACK);
    UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_WHITE);

    UTIL_LCD_DisplayStringAt(TRAIL_GUI_TITLE_X,
                             TRAIL_GUI_TITLE_Y,
                             (uint8_t*)TRAIL_GUI_TITLE,
                             LEFT_MODE);

    UTIL_LCD_DrawHLine(TRAIL_GUI_TITLE_X,
                       TRAIL_GUI_TITLE_Y + Font24.Height + 1U,
                       TRAIL_GUI_TITLE_CHAR_COUNT * Font24.Width,
                       UTIL_LCD_COLOR_BLACK);
}

void TrailGui_DrawVerticalSplitLine(void)
{
    UTIL_LCD_FillRect(TRAIL_GUI_SPLIT_LINE_X,
                      0U,
                      TRAIL_GUI_LINE_WIDTH_THIN,
                      TRAIL_GUI_SCREEN_HEIGHT,
                      UTIL_LCD_COLOR_BLACK);
}

void TrailGui_DrawCornerBoundingRectangle(const TrailGui_Point points[TRAIL_GUI_CORNER_COUNT],
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
    uint32_t i;

    if ((points == NULL) || (corner_length_px == 0U))
    {
        return;
    }

    min_x = points[0].x;
    max_x = points[0].x;
    min_y = points[0].y;
    max_y = points[0].y;

    for (i = 1U; i < TRAIL_GUI_CORNER_COUNT; i++)
    {
        min_x = TrailGui_MinU16(min_x, points[i].x);
        max_x = TrailGui_MaxU16(max_x, points[i].x);
        min_y = TrailGui_MinU16(min_y, points[i].y);
        max_y = TrailGui_MaxU16(max_y, points[i].y);
    }

    if ((max_x <= min_x) || (max_y <= min_y))
    {
        return;
    }

    width_px = max_x - min_x;
    height_px = max_y - min_y;
    length_px = TrailGui_ClampCornerLength(corner_length_px, width_px, height_px);

    if (length_px == 0U)
    {
        return;
    }

    /* Top-left corner. */
    UTIL_LCD_DrawHLine(min_x, min_y, length_px, color);
    UTIL_LCD_DrawVLine(min_x, min_y, length_px, color);

    /* Top-right corner. */
    UTIL_LCD_DrawHLine((uint32_t)(max_x - length_px), min_y, length_px, color);
    UTIL_LCD_DrawVLine(max_x, min_y, length_px, color);

    /* Bottom-left corner. */
    UTIL_LCD_DrawHLine(min_x, max_y, length_px, color);
    UTIL_LCD_DrawVLine(min_x, (uint32_t)(max_y - length_px), length_px, color);

    /* Bottom-right corner. */
    UTIL_LCD_DrawHLine((uint32_t)(max_x - length_px), max_y, length_px, color);
    UTIL_LCD_DrawVLine(max_x, (uint32_t)(max_y - length_px), length_px, color);
}

void TrailGui_FillRoundedRectangle(const TrailGui_Point points[TRAIL_GUI_CORNER_COUNT],
                                   uint16_t radius_px,
                                   uint32_t color)
{
    uint16_t min_x;
    uint16_t max_x;
    uint16_t min_y;
    uint16_t max_y;
    uint16_t width_px;
    uint16_t height_px;
    uint16_t clamped_radius_px;
    uint16_t row;
    uint32_t i;

    if (points == NULL)
    {
        return;
    }

    min_x = points[0].x;
    max_x = points[0].x;
    min_y = points[0].y;
    max_y = points[0].y;

    for (i = 1U; i < TRAIL_GUI_CORNER_COUNT; i++)
    {
        min_x = TrailGui_MinU16(min_x, points[i].x);
        max_x = TrailGui_MaxU16(max_x, points[i].x);
        min_y = TrailGui_MinU16(min_y, points[i].y);
        max_y = TrailGui_MaxU16(max_y, points[i].y);
    }

    if ((max_x <= min_x) || (max_y <= min_y))
    {
        return;
    }

    width_px = (uint16_t)(max_x - min_x + 1U);
    height_px = (uint16_t)(max_y - min_y + 1U);
    clamped_radius_px = TrailGui_ClampToHalfExtents(radius_px, width_px, height_px);

    if (clamped_radius_px == 0U)
    {
        UTIL_LCD_FillRect(min_x, min_y, width_px, height_px, color);
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
        else if (row >= (height_px - clamped_radius_px))
        {
            distance_px = (uint16_t)(row - (height_px - clamped_radius_px - 1U));
            inset_px = TrailGui_CircleInsetForRow(clamped_radius_px, distance_px);
        }

        if ((inset_px * 2U) >= width_px)
        {
            continue;
        }

        line_width_px = (uint16_t)(width_px - (2U * inset_px));

        UTIL_LCD_DrawHLine((uint32_t)(min_x + inset_px),
                           (uint32_t)(min_y + row),
                           line_width_px,
                           color);
    }
}

void TrailGui_DrawDefaultScreen(void)
{
    TrailGui_ClearScreen(TRAIL_GUI_COLOR_LIGHT_OLIVE);
    TrailGui_DrawVerticalSplitLine();
    TrailGui_Point rectangle_points[TRAIL_GUI_CORNER_COUNT] = {
        {4U,   42U},
        {235U, 42U},
        {235U, 268U},
        {4U,   268U}
    };

    TrailGui_DrawCornerBoundingRectangle(rectangle_points, 20U, UTIL_LCD_COLOR_BLACK);
    TrailGui_FillRoundedRectangle(rectangle_points, 20U,UTIL_LCD_COLOR_WHITE);

    TrailGui_DrawTitleText();
}
