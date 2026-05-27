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

static uint16_t TrailGui_GetBoundingBoxWidth(const TrailGui_BoundingBox* bounding_box)
{
    return (uint16_t)(bounding_box->x_max - bounding_box->x_min + 1U);
}

static uint16_t TrailGui_GetBoundingBoxHeight(const TrailGui_BoundingBox* bounding_box)
{
    return (uint16_t)(bounding_box->y_max - bounding_box->y_min + 1U);
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

void TrailGui_ClearScreen(uint32_t color)
{
    UTIL_LCD_Clear(color);
}

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
}
