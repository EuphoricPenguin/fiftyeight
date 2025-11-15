#include "widgets.h"
#include <pebble.h>

// Global widget configuration
static WidgetConfig s_widget_config = {
    .top_left_widget = WIDGET_MONTH_DATE,
    .top_right_widget = WIDGET_DAY_DATE
};

// Battery and health data
static int s_battery_percent = 100;
static int s_step_count = 0;
static int s_step_goal = 10000; // Default step goal

// Sprite sheets
static GBitmap *s_battery_sprites = NULL;
static GBitmap *s_steps_sprites = NULL;
static GBitmap *s_date_sprites = NULL;
static GBitmap *s_am_pm_indicator = NULL;

// External settings (these will be linked from the main file)
extern bool s_settings_use_24_hour_format;
extern bool s_settings_dark_mode;

// Function to invert bitmap palette for dark mode
static void invert_bitmap_palette(GBitmap *bitmap) {
    if (!bitmap) return;
    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) return;
    // Get the bitmap format to determine palette size
    GBitmapFormat format = gbitmap_get_format(bitmap);
    int palette_size = 0;
    switch (format) {
        case GBitmapFormat1BitPalette:
            palette_size = 2;
            break;
        case GBitmapFormat2BitPalette:
            palette_size = 4;
            break;
        case GBitmapFormat4BitPalette:
            palette_size = 16;
            break;
        default:
            // Not a palette-based format, can't invert
            return;
    }
    // Invert the palette colors
    for (int i = 0; i < palette_size; i++) {
        if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = GColorWhite;
        } else if (gcolor_equal(palette[i], GColorWhite)) {
            palette[i] = GColorBlack;
        }
    }
}

// Battery state handler
static void battery_state_handler(BatteryChargeState charge_state) {
    s_battery_percent = charge_state.charge_percent;
    // Force redraw to update battery indicator
    Layer *root_layer = window_get_root_layer(window_stack_get_top_window());
    if (root_layer) {
        layer_mark_dirty(root_layer);
    }
}

// Health event handler
static void health_event_handler(HealthEventType event, void *context) {
    if (event == HealthEventSignificantUpdate || event == HealthEventMovementUpdate) {
        // Update step count for current day using Pebble SDK's time_start_of_today()
        time_t start = time_start_of_today();
        time_t end = start + SECONDS_PER_DAY - 1; // End of day (11:59:59 PM)
        
        // Get steps for current day only
        HealthValue steps = health_service_sum(HealthMetricStepCount, start, end);
        s_step_count = (int)steps;
        
        // Force redraw to update step counter
        Layer *root_layer = window_get_root_layer(window_stack_get_top_window());
        if (root_layer) {
            layer_mark_dirty(root_layer);
        }
    }
}

// Function to draw a date number (digits from date.png)
static void draw_date_number(GContext *ctx, int digit, int x, int y) {
    // Validate sprite sheet exists
    if (!s_date_sprites) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Date sprite sheet is NULL");
        return;
    }
    // Validate sprite sheet bounds
    GSize sprite_sheet_size = gbitmap_get_bounds(s_date_sprites).size;
    if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid date sprite sheet dimensions: %dx%d",
                sprite_sheet_size.w, sprite_sheet_size.h);
        return;
    }
    // Map digit to sprite position in the 3x4 grid
    // Layout: 1,2,3,4,5,6,7,8,9,0
    int sprite_index = -1;
    switch (digit) {
        case 1: sprite_index = 0; break;
        case 2: sprite_index = 1; break;
        case 3: sprite_index = 2; break;
        case 4: sprite_index = 3; break;
        case 5: sprite_index = 4; break;
        case 6: sprite_index = 5; break;
        case 7: sprite_index = 6; break;
        case 8: sprite_index = 7; break;
        case 9: sprite_index = 8; break;
        case 0: sprite_index = 9; break;
        default:
            APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown date digit: %d", digit);
            return;
    }
    // Calculate sprite position in the spritesheet
    int sprite_row = sprite_index / DATE_SPRITES_PER_ROW;
    int sprite_col = sprite_index % DATE_SPRITES_PER_ROW;
    // Validate sprite position is within bounds
    int max_col = sprite_sheet_size.w / DATE_WIDTH;
    int max_row = sprite_sheet_size.h / DATE_HEIGHT;
    if (sprite_col >= max_col || sprite_row >= max_row) {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Date sprite position out of bounds: digit=%d, row=%d/%d, col=%d/%d",
                digit, sprite_row, max_row, sprite_col, max_col);
        return;
    }
    // Calculate source rectangle in the spritesheet
    GRect source_rect = GRect(
                            sprite_col * DATE_WIDTH,
                            sprite_row * DATE_HEIGHT,
                            DATE_WIDTH,
                            DATE_HEIGHT
                        );
    // Calculate destination position
    GRect dest_rect = GRect(x, y, DATE_WIDTH, DATE_HEIGHT);
    // Set compositing mode for transparency
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    // Create a sub-bitmap for the specific sprite
    GBitmap *digit_bitmap = gbitmap_create_as_sub_bitmap(s_date_sprites, source_rect);
    if (!digit_bitmap) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub-bitmap for date digit %d", digit);
        return;
    }
    // Draw the sprite
    graphics_draw_bitmap_in_rect(ctx, digit_bitmap, dest_rect);
    // Clean up the sub-bitmap
    gbitmap_destroy(digit_bitmap);
}

// Initialize widget system
void widgets_init(void) {
    // Load sprite sheets
    s_battery_sprites = gbitmap_create_with_resource(RESOURCE_ID_BATTERY);
    s_steps_sprites = gbitmap_create_with_resource(RESOURCE_ID_STEPS);
    s_date_sprites = gbitmap_create_with_resource(RESOURCE_ID_DATE_SPRITES);
    s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
    
    // Invert palette colors for dark mode if enabled
    if (s_settings_dark_mode) {
        invert_bitmap_palette(s_battery_sprites);
        invert_bitmap_palette(s_steps_sprites);
        invert_bitmap_palette(s_date_sprites);
        invert_bitmap_palette(s_am_pm_indicator);
    }
    
    // Subscribe to battery state updates
    battery_state_service_subscribe(battery_state_handler);
    battery_state_handler(battery_state_service_peek());
    
    // Subscribe to health service updates
    health_service_events_subscribe(health_event_handler, NULL);
    
    // Get initial step count for current day using Pebble SDK's time_start_of_today()
    time_t start = time_start_of_today();
    time_t end = start + SECONDS_PER_DAY - 1; // End of day (11:59:59 PM)
    
    // Get steps for current day only
    HealthValue steps = health_service_sum(HealthMetricStepCount, start, end);
    s_step_count = (int)steps;
}

// Reload widget sprites (for dark mode changes)
void widgets_reload_sprites(void) {
    // Clean up existing sprite sheets
    if (s_battery_sprites) {
        gbitmap_destroy(s_battery_sprites);
        s_battery_sprites = NULL;
    }
    if (s_steps_sprites) {
        gbitmap_destroy(s_steps_sprites);
        s_steps_sprites = NULL;
    }
    if (s_date_sprites) {
        gbitmap_destroy(s_date_sprites);
        s_date_sprites = NULL;
    }
    if (s_am_pm_indicator) {
        gbitmap_destroy(s_am_pm_indicator);
        s_am_pm_indicator = NULL;
    }
    
    // Reload all sprite sheets
    s_battery_sprites = gbitmap_create_with_resource(RESOURCE_ID_BATTERY);
    s_steps_sprites = gbitmap_create_with_resource(RESOURCE_ID_STEPS);
    s_date_sprites = gbitmap_create_with_resource(RESOURCE_ID_DATE_SPRITES);
    s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
    
    // Invert palette colors for dark mode if enabled
    if (s_settings_dark_mode) {
        invert_bitmap_palette(s_battery_sprites);
        invert_bitmap_palette(s_steps_sprites);
        invert_bitmap_palette(s_date_sprites);
        invert_bitmap_palette(s_am_pm_indicator);
    }
}

// Deinitialize widget system
void widgets_deinit(void) {
    // Unsubscribe from services
    battery_state_service_unsubscribe();
    health_service_events_unsubscribe();
    
    // Clean up sprite sheets
    if (s_battery_sprites) {
        gbitmap_destroy(s_battery_sprites);
        s_battery_sprites = NULL;
    }
    if (s_steps_sprites) {
        gbitmap_destroy(s_steps_sprites);
        s_steps_sprites = NULL;
    }
    if (s_date_sprites) {
        gbitmap_destroy(s_date_sprites);
        s_date_sprites = NULL;
    }
    if (s_am_pm_indicator) {
        gbitmap_destroy(s_am_pm_indicator);
        s_am_pm_indicator = NULL;
    }
}

// Set widget configuration
void widgets_set_config(WidgetConfig config) {
    s_widget_config = config;
    APP_LOG(APP_LOG_LEVEL_INFO, "Widget config updated: top_left=%d, top_right=%d", 
            s_widget_config.top_left_widget, s_widget_config.top_right_widget);
}

// Draw month date widget
static void draw_month_date_widget(GContext *ctx, int x, int y, struct tm *tick_time) {
    int month = tick_time->tm_mon + 1; // Convert from 0-based to 1-based
    
    // Draw month using existing date sprites
    // For single-digit months, we need to handle the positioning
    if (month < 10) {
        // Single digit month - draw at x position
        draw_date_number(ctx, month, x, y);
    } else {
        // Two-digit month - draw tens and ones digits with 4px spacing
        int month_tens = month / 10;
        int month_ones = month % 10;
        draw_date_number(ctx, month_tens, x, y);
        draw_date_number(ctx, month_ones, x + DATE_WIDTH + 4, y); // Add 4px spacing
    }
}

// Draw day date widget  
static void draw_day_date_widget(GContext *ctx, int x, int y, struct tm *tick_time) {
    int day = tick_time->tm_mday;
    
    // Draw day using existing date sprites
    // For single-digit days, we need to handle the positioning
    if (day < 10) {
        // Single digit day - draw at x position
        draw_date_number(ctx, day, x, y);
    } else {
        // Two-digit day - draw tens and ones digits with 4px spacing
        int day_tens = day / 10;
        int day_ones = day % 10;
        draw_date_number(ctx, day_tens, x, y);
        draw_date_number(ctx, day_ones, x + DATE_WIDTH + 4, y); // Add 4px spacing
    }
}

// Draw AM/PM indicator widget
static void draw_am_pm_widget(GContext *ctx, int x, int y, struct tm *tick_time) {
    if (!s_am_pm_indicator) {
        return;
    }
    
    bool is_pm = (tick_time->tm_hour >= 12);
    
    // AM/PM indicator sprite dimensions: 20x14, 1 column, 2 rows
    // Row 0: "P" (PM), Row 1: "A" (AM)
    int sprite_width = 20;
    int sprite_height = 14;
    int frame_index = is_pm ? 0 : 1; // 0 = PM ("P"), 1 = AM ("A")
    
    GRect source_rect = GRect(0, frame_index * sprite_height, sprite_width, sprite_height);
    GRect dest_rect = GRect(x, y, sprite_width, sprite_height);
    
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    GBitmap *sprite_bitmap = gbitmap_create_as_sub_bitmap(s_am_pm_indicator, source_rect);
    if (sprite_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
        gbitmap_destroy(sprite_bitmap);
    }
}

// Draw battery indicator widget
static void draw_battery_widget(GContext *ctx, int x, int y) {
    if (!s_battery_sprites) return;
    
    // Calculate which sprite frame to use based on 10% segments
    // 10 sprites total: 0 (full) to 9 (empty)
    // Simple 10% increment logic: 100-90, 90-80, 80-70, etc.
    int frame_index;
    if (s_battery_percent >= 90) frame_index = 0;      // 90-100%: full battery
    else if (s_battery_percent >= 80) frame_index = 1; // 80-89%: next level
    else if (s_battery_percent >= 70) frame_index = 2; // 70-79%: next level
    else if (s_battery_percent >= 60) frame_index = 3; // 60-69%: next level
    else if (s_battery_percent >= 50) frame_index = 4; // 50-59%: next level
    else if (s_battery_percent >= 40) frame_index = 5; // 40-49%: next level
    else if (s_battery_percent >= 30) frame_index = 6; // 30-39%: next level
    else if (s_battery_percent >= 20) frame_index = 7; // 20-29%: next level
    else if (s_battery_percent >= 10) frame_index = 8; // 10-19%: next level
    else frame_index = 9;                              // 0-9%: empty battery
    
    // Battery sprite dimensions: 44x14, 1 column, 10 rows
    int sprite_height = 14;
    GRect source_rect = GRect(0, frame_index * sprite_height, 44, sprite_height);
    GRect dest_rect = GRect(x, y, 44, sprite_height);
    
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    GBitmap *sprite_bitmap = gbitmap_create_as_sub_bitmap(s_battery_sprites, source_rect);
    if (sprite_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
        gbitmap_destroy(sprite_bitmap);
    }
}

// Draw step count widget
static void draw_steps_widget(GContext *ctx, int x, int y) {
    if (!s_steps_sprites) return;
    
    // Calculate which sprite frame to use based on step progression
    // Each frame represents 1/9th of the step goal
    // Top frame (0) = no steps, bottom frame (8) = 100% complete
    int frame_index;
    if (s_step_count >= s_step_goal) frame_index = 8; // Full/complete (bottom)
    else if (s_step_count >= (s_step_goal * 8/9)) frame_index = 7;
    else if (s_step_count >= (s_step_goal * 7/9)) frame_index = 6;
    else if (s_step_count >= (s_step_goal * 6/9)) frame_index = 5;
    else if (s_step_count >= (s_step_goal * 5/9)) frame_index = 4;
    else if (s_step_count >= (s_step_goal * 4/9)) frame_index = 3;
    else if (s_step_count >= (s_step_goal * 3/9)) frame_index = 2;
    else if (s_step_count >= (s_step_goal * 2/9)) frame_index = 1;
    else if (s_step_count >= (s_step_goal * 1/9)) frame_index = 0;
    else frame_index = 0; // No steps (top)
    
    // Steps sprite dimensions: 44x14, 1 column, 9 rows
    int sprite_height = 14;
    GRect source_rect = GRect(0, frame_index * sprite_height, 44, sprite_height);
    GRect dest_rect = GRect(x, y, 44, sprite_height);
    
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    GBitmap *sprite_bitmap = gbitmap_create_as_sub_bitmap(s_steps_sprites, source_rect);
    if (sprite_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
        gbitmap_destroy(sprite_bitmap);
    }
}

// Draw a widget in the specified corner
void widgets_draw_corner(GContext *ctx, CornerPosition corner, struct tm *tick_time) {
    WidgetType widget_type;
    int padding_top = 10;
    int padding_side = 10;
    
    // Determine which widget to draw based on corner position
    if (corner == CORNER_TOP_LEFT) {
        widget_type = s_widget_config.top_left_widget;
    } else {
        widget_type = s_widget_config.top_right_widget;
    }
    
    // Debug logging
    APP_LOG(APP_LOG_LEVEL_INFO, "Drawing corner %d, widget type: %d", corner, widget_type);
    
    // Skip if no widget selected
    if (widget_type == WIDGET_NONE) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Skipping corner %d - no widget selected", corner);
        return;
    }
    
    // Calculate position
    int x, y = padding_top;
    GRect bounds = layer_get_bounds(window_get_root_layer(window_stack_get_top_window()));
    
    if (corner == CORNER_TOP_LEFT) {
        x = padding_side;
    } else {
        // For right corner, we need to calculate based on widget width
        int widget_width = 0;
        switch (widget_type) {
            case WIDGET_MONTH_DATE:
                widget_width = (tick_time->tm_mon + 1 < 10) ? DATE_WIDTH : (DATE_WIDTH * 2 + 4);
                break;
            case WIDGET_DAY_DATE:
                widget_width = (tick_time->tm_mday < 10) ? DATE_WIDTH : (DATE_WIDTH * 2 + 4);
                break;
            case WIDGET_AM_PM_INDICATOR:
                widget_width = 20;
                break;
            case WIDGET_BATTERY_INDICATOR:
            case WIDGET_STEP_COUNT:
                widget_width = 44;
                break;
            default:
                widget_width = 30;
        }
        x = bounds.size.w - widget_width - padding_side;
    }
    
    // Draw the selected widget
    switch (widget_type) {
        case WIDGET_MONTH_DATE:
            draw_month_date_widget(ctx, x, y, tick_time);
            break;
        case WIDGET_DAY_DATE:
            draw_day_date_widget(ctx, x, y, tick_time);
            break;
        case WIDGET_AM_PM_INDICATOR:
            draw_am_pm_widget(ctx, x, y, tick_time);
            break;
        case WIDGET_BATTERY_INDICATOR:
            draw_battery_widget(ctx, x, y);
            break;
        case WIDGET_STEP_COUNT:
            draw_steps_widget(ctx, x, y);
            break;
        default:
            break;
    }
}

// Handle battery updates (external call)
void widgets_handle_battery_update(void) {
    battery_state_handler(battery_state_service_peek());
}

// Set step goal
void widgets_set_step_goal(int step_goal) {
    if (step_goal > 0) {
        s_step_goal = step_goal;
        APP_LOG(APP_LOG_LEVEL_INFO, "Step goal updated to: %d", s_step_goal);
    }
}

// Handle health updates (external call)
void widgets_handle_health_update(void) {
    // Update step count for current day using Pebble SDK's time_start_of_today()
    time_t start = time_start_of_today();
    time_t end = start + SECONDS_PER_DAY - 1; // End of day (11:59:59 PM)
    
    // Get steps for current day only
    HealthValue steps = health_service_sum(HealthMetricStepCount, start, end);
    s_step_count = (int)steps;
}
