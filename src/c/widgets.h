#ifndef WIDGETS_H
#define WIDGETS_H

#include <pebble.h>

// Widget types
typedef enum {
    WIDGET_NONE = 0,
    WIDGET_MONTH_DATE,
    WIDGET_DAY_DATE,
    WIDGET_AM_PM_INDICATOR,
    WIDGET_BATTERY_INDICATOR,
    WIDGET_STEP_COUNT
} WidgetType;

// Corner positions
typedef enum {
    CORNER_TOP_LEFT = 0,
    CORNER_TOP_RIGHT
} CornerPosition;

// Widget configuration structure
typedef struct {
    WidgetType top_left_widget;
    WidgetType top_right_widget;
} WidgetConfig;

// Settings struct for persistent storage
typedef struct Settings
{
    bool dark_mode;
    bool use_24_hour_format;
    bool use_two_letter_day;
    bool debug_mode;
    bool debug_logging;
    bool show_second_dot;
    bool show_hour_minute_dots;
    int step_goal;
    WidgetConfig widget_config;
} Settings;

// Function declarations
void widgets_init(void);
void widgets_deinit(void);
void widgets_set_config(WidgetConfig config);
void widgets_draw_corner(GContext *ctx, CornerPosition corner, struct tm *tick_time);
void widgets_handle_battery_update(void);
void widgets_handle_health_update(void);
void widgets_set_step_goal(int step_goal);
void widgets_reload_sprites(void);


// Sprite sheet dimensions
#define DATE_WIDTH 20
#define DATE_HEIGHT 14
#define DATE_SPRITES_PER_ROW 3

// External access to settings
extern bool s_settings_show_am_pm;
extern bool s_settings_use_24_hour_format;
extern bool s_settings_dark_mode;
extern bool s_settings_debug_logging;

#endif // WIDGETS_H
