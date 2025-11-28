#ifndef CONFIG_H
#define CONFIG_H

#include "widgets.h"

// Default settings for new users
#define DEFAULT_DARK_MODE false
#define DEFAULT_USE_24_HOUR_FORMAT false
#define DEFAULT_USE_TWO_LETTER_DAY false
#define DEFAULT_DEBUG_MODE false
#define DEFAULT_DEBUG_LOGGING false
#define DEFAULT_SHOW_SECOND_DOT false
#define DEFAULT_SHOW_HOUR_MINUTE_DOTS true
#define DEFAULT_STEP_GOAL 10000
#define DEFAULT_TOP_LEFT_WIDGET WIDGET_DAY_DATE
#define DEFAULT_TOP_RIGHT_WIDGET WIDGET_BATTERY_INDICATOR

// Function to get default widget configuration
static inline WidgetConfig get_default_widget_config() {
    WidgetConfig config = {
        .top_left_widget = DEFAULT_TOP_LEFT_WIDGET,
        .top_right_widget = DEFAULT_TOP_RIGHT_WIDGET
    };
    return config;
}

// Function to get default settings
static inline Settings get_default_settings() {
    Settings settings = {
        .dark_mode = DEFAULT_DARK_MODE,
        .use_24_hour_format = DEFAULT_USE_24_HOUR_FORMAT,
        .use_two_letter_day = DEFAULT_USE_TWO_LETTER_DAY,
        .debug_mode = DEFAULT_DEBUG_MODE,
        .debug_logging = DEFAULT_DEBUG_LOGGING,
        .show_second_dot = DEFAULT_SHOW_SECOND_DOT,
        .show_hour_minute_dots = DEFAULT_SHOW_HOUR_MINUTE_DOTS,
        .step_goal = DEFAULT_STEP_GOAL,
        .widget_config = get_default_widget_config()
    };
    return settings;
}

#endif // CONFIG_H
