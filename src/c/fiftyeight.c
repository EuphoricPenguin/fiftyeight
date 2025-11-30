#include <pebble.h>
#include "math.h"
#include "widgets.h"
#include "config.h"

static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_priority_sprites;
static GBitmap *s_subpriority_sprites;
static GBitmap *s_midpriority_sprites;
static GBitmap *s_day_sprites;

// Debug mode variables
static int s_debug_counter = 0;
static AppTimer *s_debug_timer = NULL;

// Forward declarations
static void debug_timer_callback(void *data);

// Persistent storage key
#define SETTINGS_KEY 1

// Message keys for Clay configuration
#define MESSAGE_KEY_ShowSecondDot 10007
#define MESSAGE_KEY_ShowHourMinuteDots 10008

// External settings for widget system
bool s_settings_dark_mode = false;
bool s_settings_debug_logging = false;


static Settings s_settings;

// Function to save settings to persistent storage
static void prv_save_settings()
{
    persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

// Function to load settings from persistent storage
static void prv_load_settings()
{
    if (persist_exists(SETTINGS_KEY))
    {
        persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
    }
}


// Function to invert bitmap palette for dark mode
static void invert_bitmap_palette(GBitmap *bitmap);

// Function to reload sprites with correct palette for current dark mode setting
static void prv_reload_sprites()
{
    // Clean up existing sprites
    if (s_priority_sprites) gbitmap_destroy(s_priority_sprites);
    if (s_subpriority_sprites) gbitmap_destroy(s_subpriority_sprites);
    if (s_midpriority_sprites) gbitmap_destroy(s_midpriority_sprites);
    if (s_day_sprites) gbitmap_destroy(s_day_sprites);
    // Reload all sprite sheets
    s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
    s_subpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_SUBPRIORITY_DIGIT);
    s_midpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_MIDPRIORITY_DIGIT);
    s_day_sprites = gbitmap_create_with_resource(RESOURCE_ID_DAY_SPRITES);
    // Invert palette colors for dark mode if enabled
    if (s_settings.dark_mode)
    {
        invert_bitmap_palette(s_priority_sprites);
        invert_bitmap_palette(s_subpriority_sprites);
        invert_bitmap_palette(s_midpriority_sprites);
        invert_bitmap_palette(s_day_sprites);
    }
}

// AppMessage inbox received handler
static void prv_inbox_received_handler(DictionaryIterator *iter, void *context)
{
    bool dark_mode_changed = false;
    // Read settings from Clay configuration
    Tuple *dark_mode_t = dict_find(iter, MESSAGE_KEY_DarkMode);
    if (dark_mode_t)
    {
        bool new_dark_mode = dark_mode_t->value->int32 == 1;
        dark_mode_changed = (s_settings.dark_mode != new_dark_mode);
        s_settings.dark_mode = new_dark_mode;
    }
    Tuple *use_24_hour_format_t = dict_find(iter, MESSAGE_KEY_Use24HourFormat);
    if (use_24_hour_format_t)
    {
        s_settings.use_24_hour_format = use_24_hour_format_t->value->int32 == 1;
    }
    Tuple *use_two_letter_day_t = dict_find(iter, MESSAGE_KEY_UseTwoLetterDay);
    if (use_two_letter_day_t)
    {
        s_settings.use_two_letter_day = use_two_letter_day_t->value->int32 == 1;
    }
    
    // Handle new dot visibility settings
    Tuple *show_second_dot_t = dict_find(iter, MESSAGE_KEY_ShowSecondDot);
    if (show_second_dot_t) {
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "ShowSecondDot received - type: %d", show_second_dot_t->type);
        }
        bool new_show_second_dot;
        if (show_second_dot_t->type == TUPLE_CSTRING) {
            // Convert string to boolean
            const char *show_second_dot_str = show_second_dot_t->value->cstring;
            if (s_settings.debug_logging) {
                APP_LOG(APP_LOG_LEVEL_INFO, "ShowSecondDot as string: '%s'", show_second_dot_str);
            }
            new_show_second_dot = (strcmp(show_second_dot_str, "true") == 0 || strcmp(show_second_dot_str, "1") == 0);
        } else {
            // Use integer value directly
            new_show_second_dot = show_second_dot_t->value->int32 == 1;
        }
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "ShowSecondDot setting changed: %d -> %d", s_settings.show_second_dot, new_show_second_dot);
        }
        s_settings.show_second_dot = new_show_second_dot;
    }
    
    Tuple *show_hour_minute_dots_t = dict_find(iter, MESSAGE_KEY_ShowHourMinuteDots);
    if (show_hour_minute_dots_t) {
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "ShowHourMinuteDots received - type: %d", show_hour_minute_dots_t->type);
        }
        bool new_show_hour_minute_dots;
        if (show_hour_minute_dots_t->type == TUPLE_CSTRING) {
            // Convert string to boolean
            const char *show_hour_minute_dots_str = show_hour_minute_dots_t->value->cstring;
            if (s_settings.debug_logging) {
                APP_LOG(APP_LOG_LEVEL_INFO, "ShowHourMinuteDots as string: '%s'", show_hour_minute_dots_str);
            }
            new_show_hour_minute_dots = (strcmp(show_hour_minute_dots_str, "true") == 0 || strcmp(show_hour_minute_dots_str, "1") == 0);
        } else {
            // Use integer value directly
            new_show_hour_minute_dots = show_hour_minute_dots_t->value->int32 == 1;
        }
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "ShowHourMinuteDots setting changed: %d -> %d", s_settings.show_hour_minute_dots, new_show_hour_minute_dots);
        }
        s_settings.show_hour_minute_dots = new_show_hour_minute_dots;
    }
    
    // Handle step goal configuration
    Tuple *step_goal_t = dict_find(iter, MESSAGE_KEY_StepGoal);
    if (step_goal_t) {
        // Handle both string and integer values from Clay
        int32_t step_goal_value;
        if (step_goal_t->type == TUPLE_CSTRING) {
            // Convert string to integer with better error handling
            const char *step_goal_str = step_goal_t->value->cstring;
            step_goal_value = atoi(step_goal_str);
            if (s_settings.debug_logging) {
                APP_LOG(APP_LOG_LEVEL_INFO, "Received step_goal as string: '%s' -> %ld", step_goal_str, (long)step_goal_value);
            }
            
            // Validate the conversion
            if (step_goal_value <= 0) {
                if (s_settings.debug_logging) {
                    APP_LOG(APP_LOG_LEVEL_WARNING, "Invalid step goal conversion, using default: %ld", (long)step_goal_value);
                }
                step_goal_value = 10000; // Default step goal
            }
        } else {
            // Use integer value directly
            step_goal_value = step_goal_t->value->int32;
            if (s_settings.debug_logging) {
                APP_LOG(APP_LOG_LEVEL_INFO, "Received step_goal as int: %ld (type: %d)", (long)step_goal_value, step_goal_t->type);
            }
        }
        // Save step goal to settings
        s_settings.step_goal = step_goal_value;
        // Update widget system with new step goal
        widgets_set_step_goal(step_goal_value);
    } else {
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "No step_goal received, using saved value: %d", s_settings.step_goal);
        }
    }
    
    // Handle widget configuration
    Tuple *top_left_widget_t = dict_find(iter, MESSAGE_KEY_TopLeftWidget);
    if (top_left_widget_t) {
        // Handle both string and integer values from Clay
        int32_t widget_value;
        if (top_left_widget_t->type == TUPLE_CSTRING) {
            // Convert string to integer
            widget_value = atoi(top_left_widget_t->value->cstring);
        } else {
            // Use integer value directly
            widget_value = top_left_widget_t->value->int32;
        }
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "Received top_left_widget: %ld (type: %d)", (long)widget_value, top_left_widget_t->type);
        }
        s_settings.widget_config.top_left_widget = (WidgetType)widget_value;
    } else {
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "No top_left_widget received, using default");
        }
        s_settings.widget_config.top_left_widget = WIDGET_MONTH_DATE;
    }
    
    Tuple *top_right_widget_t = dict_find(iter, MESSAGE_KEY_TopRightWidget);
    if (top_right_widget_t) {
        // Handle both string and integer values from Clay
        int32_t widget_value;
        if (top_right_widget_t->type == TUPLE_CSTRING) {
            // Convert string to integer
            widget_value = atoi(top_right_widget_t->value->cstring);
        } else {
            // Use integer value directly
            widget_value = top_right_widget_t->value->int32;
        }
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "Received top_right_widget: %ld (type: %d)", (long)widget_value, top_right_widget_t->type);
        }
        s_settings.widget_config.top_right_widget = (WidgetType)widget_value;
    } else {
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "No top_right_widget received, using default");
        }
        s_settings.widget_config.top_right_widget = WIDGET_DAY_DATE;
    }
    
    // Update widget configuration
    widgets_set_config(s_settings.widget_config);
    
    // Update widget system settings
    s_settings_dark_mode = s_settings.dark_mode;
    
    // Save settings to persistent storage
    prv_save_settings();
    // If dark mode changed, reload sprites with correct palette
    if (dark_mode_changed)
    {
        prv_reload_sprites();
        widgets_reload_sprites();
    }
    // Force redraw to apply new settings
    layer_mark_dirty(s_canvas_layer);
}

// Debug mode timer callback
static void debug_timer_callback(void *data) {
    if (s_settings.debug_mode) {
        s_debug_counter++;
        if (s_debug_counter > 100) { // Reset after cycling through all combinations
            s_debug_counter = 0;
        }
        layer_mark_dirty(s_canvas_layer);
        // Schedule next debug update (500ms interval for quick cycling)
        s_debug_timer = app_timer_register(500, debug_timer_callback, NULL);
    } else {
        s_debug_timer = NULL;
    }
}

// Rotating dot variables
static int s_current_second = 0;
static int s_current_minute = 0;
static int s_current_hour = 0;

// Function to invert bitmap palette for dark mode
static void invert_bitmap_palette(GBitmap *bitmap)
{
    if (!bitmap) return;
    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) return;
    // Get the bitmap format to determine palette size
    GBitmapFormat format = gbitmap_get_format(bitmap);
    int palette_size = 0;
    switch (format)
    {
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
    for (int i = 0; i < palette_size; i++)
    {
        if (gcolor_equal(palette[i], GColorBlack))
        {
            palette[i] = GColorWhite;
        }
        else if (gcolor_equal(palette[i], GColorWhite))
        {
            palette[i] = GColorBlack;
        }
    }
}

// Sprite sheet dimensions
#define PRIORITY_WIDTH 40
#define SUBPRIORITY_WIDTH 27
#define MIDPRIORITY_WIDTH 34
#define SPRITE_HEIGHT 18
#define SPRITES_PER_ROW 3
#define SPRITES_PER_COLUMN 4

// Day sprite dimensions (day.png - 4x4 grid, 20x14 sprites)
#define DAY_WIDTH 20
#define DAY_HEIGHT 14
#define DAY_SPRITES_PER_ROW 4

// Date sprite dimensions (date.png - 3x4 grid, 20x14 sprites)
#define DATE_WIDTH 20
#define DATE_HEIGHT 14
#define DATE_SPRITES_PER_ROW 3


// Function to draw a day character (letters from day.png)
static void draw_day_char(GContext *ctx, char character, int x, int y)
{
    // Validate sprite sheet exists
    if (!s_day_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Day sprite sheet is NULL");
        return;
    }
    // Validate sprite sheet bounds
    GSize sprite_sheet_size = gbitmap_get_bounds(s_day_sprites).size;
    if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid day sprite sheet dimensions: %dx%d",
                sprite_sheet_size.w, sprite_sheet_size.h);
        return;
    }
    // Map character to sprite position in the 4x4 grid
    // Layout: A,D,E,F,H,I,M,N,O,R,S,T,U,W
    int sprite_index = -1;
    switch (character)
    {
        case 'A': sprite_index = 0; break;
        case 'D': sprite_index = 1; break;
        case 'E': sprite_index = 2; break;
        case 'F': sprite_index = 3; break;
        case 'H': sprite_index = 4; break;
        case 'I': sprite_index = 5; break;
        case 'M': sprite_index = 6; break;
        case 'N': sprite_index = 7; break;
        case 'O': sprite_index = 8; break;
        case 'R': sprite_index = 9; break;
        case 'S': sprite_index = 10; break;
        case 'T': sprite_index = 11; break;
        case 'U': sprite_index = 12; break;
        case 'W': sprite_index = 13; break;
        default:
            APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown day character: %c", character);
            return;
    }
    // Calculate sprite position in the spritesheet
    int sprite_row = sprite_index / DAY_SPRITES_PER_ROW;
    int sprite_col = sprite_index % DAY_SPRITES_PER_ROW;
    // Validate sprite position is within bounds
    int max_col = sprite_sheet_size.w / DAY_WIDTH;
    int max_row = sprite_sheet_size.h / DAY_HEIGHT;
    if (sprite_col >= max_col || sprite_row >= max_row)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Day sprite position out of bounds: char=%c, row=%d/%d, col=%d/%d",
                character, sprite_row, max_row, sprite_col, max_col);
        return;
    }
    // Calculate source rectangle in the spritesheet
    GRect source_rect = GRect(
                            sprite_col * DAY_WIDTH,
                            sprite_row * DAY_HEIGHT,
                            DAY_WIDTH,
                            DAY_HEIGHT
                        );
    // Calculate destination position
    GRect dest_rect = GRect(x, y, DAY_WIDTH, DAY_HEIGHT);
    // Set compositing mode for transparency
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    // Create a sub-bitmap for the specific sprite
    GBitmap *char_bitmap = gbitmap_create_as_sub_bitmap(s_day_sprites, source_rect);
    if (!char_bitmap)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub-bitmap for day character %c",
                character);
        return;
    }
    // Draw the sprite
    graphics_draw_bitmap_in_rect(ctx, char_bitmap, dest_rect);
    // Clean up the sub-bitmap
    gbitmap_destroy(char_bitmap);
}




// Digit types for width selection
typedef enum
{
    DIGIT_PRIORITY,
    DIGIT_SUBPRIORITY,
    DIGIT_MIDPRIORITY
} DigitType;

// Helper function to get digit width based on type
static int get_digit_width(DigitType type)
{
    switch (type)
    {
        case DIGIT_PRIORITY:
            return PRIORITY_WIDTH;
        case DIGIT_SUBPRIORITY:
            return SUBPRIORITY_WIDTH;
        case DIGIT_MIDPRIORITY:
            return MIDPRIORITY_WIDTH;
        default:
            return SUBPRIORITY_WIDTH; // Default fallback
    }
}

// Function to draw a digit with specified type
static void draw_digit(GContext *ctx, int digit, DigitType type, int x, int y)
{
    GBitmap *sprite_sheet = NULL;
    int sprite_width = 0;
    // Select the appropriate sprite sheet and width
    switch (type)
    {
        case DIGIT_PRIORITY:
            sprite_sheet = s_priority_sprites;
            sprite_width = PRIORITY_WIDTH;
            break;
        case DIGIT_SUBPRIORITY:
            sprite_sheet = s_subpriority_sprites;
            sprite_width = SUBPRIORITY_WIDTH;
            break;
        case DIGIT_MIDPRIORITY:
            sprite_sheet = s_midpriority_sprites;
            sprite_width = MIDPRIORITY_WIDTH;
            break;
    }
    // Validate sprite sheet exists
    if (!sprite_sheet)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Sprite sheet is NULL for digit type: %d", type);
        return;
    }
    // Validate sprite sheet bounds
    GSize sprite_sheet_size = gbitmap_get_bounds(sprite_sheet).size;
    if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid sprite sheet dimensions: %dx%d",
                sprite_sheet_size.w, sprite_sheet_size.h);
        return;
    }
    // Calculate sprite position in the spritesheet
    // Handle digit 0 specially (it's in row 3, column 0)
    int sprite_row, sprite_col;
    if (digit == 0)
    {
        sprite_row = 3;
        sprite_col = 0;
    }
    else
    {
        sprite_row = (digit - 1) / SPRITES_PER_ROW;
        sprite_col = (digit - 1) % SPRITES_PER_ROW;
    }
    // Validate sprite position is within bounds
    int max_col = sprite_sheet_size.w / sprite_width;
    int max_row = sprite_sheet_size.h / SPRITE_HEIGHT;
    if (sprite_col >= max_col || sprite_row >= max_row)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Sprite position out of bounds: digit=%d, row=%d/%d, col=%d/%d",
                digit, sprite_row, max_row, sprite_col, max_col);
        return;
    }
    // Calculate source rectangle in the spritesheet
    GRect source_rect = GRect(
                            sprite_col * sprite_width,
                            sprite_row * SPRITE_HEIGHT,
                            sprite_width,
                            SPRITE_HEIGHT
                        );
    // Calculate destination position
    GRect dest_rect = GRect(x, y, sprite_width, SPRITE_HEIGHT);
    // Set compositing mode for transparency
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    // Create a sub-bitmap for the specific sprite
    GBitmap *sprite_bitmap = gbitmap_create_as_sub_bitmap(sprite_sheet,
                             source_rect);
    if (!sprite_bitmap)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub-bitmap for digit %d", digit);
        return;
    }
    // Draw the sprite
    graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
    // Clean up the sub-bitmap
    gbitmap_destroy(sprite_bitmap);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
    // Update current time values and refresh display
    if (units_changed & SECOND_UNIT)
    {
        s_current_second = tick_time->tm_sec;
        layer_mark_dirty(s_canvas_layer);
    }
    if (units_changed & MINUTE_UNIT)
    {
        s_current_minute = tick_time->tm_min;
        layer_mark_dirty(s_canvas_layer);
    }
    if (units_changed & HOUR_UNIT)
    {
        s_current_hour = tick_time->tm_hour;
        layer_mark_dirty(s_canvas_layer);
    }
}


static void canvas_update_proc(Layer *layer, GContext *ctx)
{
    // Set background color based on dark mode setting
    if (s_settings.dark_mode)
    {
        graphics_context_set_fill_color(ctx, GColorBlack);
    }
    else
    {
        graphics_context_set_fill_color(ctx, GColorWhite);
    }
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    
    // Debug mode: override time, date, and weekday with cycling values
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    int hour = tick_time->tm_hour;
    int minute = tick_time->tm_min;
    int day_of_week = tick_time->tm_wday;
    
    if (s_settings.debug_mode) {
        // Use debug counter to cycle through different combinations
        // Time combinations: 1:23, 12:34, 9:59, 10:10, etc.
        switch (s_debug_counter % 20) {
            case 0: hour = 1; minute = 23; break;
            case 1: hour = 12; minute = 34; break;
            case 2: hour = 9; minute = 59; break;
            case 3: hour = 10; minute = 10; break;
            case 4: hour = 11; minute = 11; break;
            case 5: hour = 2; minute = 22; break;
            case 6: hour = 3; minute = 33; break;
            case 7: hour = 4; minute = 44; break;
            case 8: hour = 5; minute = 55; break;
            case 9: hour = 6; minute = 6; break;
            case 10: hour = 7; minute = 17; break;
            case 11: hour = 8; minute = 28; break;
            case 12: hour = 13; minute = 45; break;
            case 13: hour = 14; minute = 56; break;
            case 14: hour = 15; minute = 7; break;
            case 15: hour = 16; minute = 18; break;
            case 16: hour = 17; minute = 29; break;
            case 17: hour = 18; minute = 40; break;
            case 18: hour = 19; minute = 51; break;
            case 19: hour = 20; minute = 2; break;
        }
        
        // Weekday combinations
        day_of_week = (s_debug_counter / 5) % 7;
    }
    
    // Convert hour based on time format setting (Clay override or system default)
    bool use_24_hour = s_settings.use_24_hour_format ? true : clock_is_24h_style();
    if (!use_24_hour)
    {
        if (hour > 12)
        {
            hour -= 12;
        }
        else if (hour == 0)
        {
            hour = 12;
        }
    }
    // Get hour digits
    int hour_tens = hour / 10;
    int hour_ones = hour % 10;
    // Get minute digits
    int minute_tens = minute / 10;
    int minute_ones = minute % 10;
    // Simplified digit logic:
    // - Single digit hours use priority (wide), minutes use midpriority
    // - All two-digit numbers use subpriority
    DigitType hour_tens_type = DIGIT_SUBPRIORITY;
    DigitType hour_ones_type = DIGIT_SUBPRIORITY;
    DigitType minute_tens_type = DIGIT_SUBPRIORITY;
    DigitType minute_ones_type = DIGIT_SUBPRIORITY;
    
    // Single digit hour (1-9) - use priority digit (wide) and midpriority for minutes
    if (hour_tens == 0)
    {
        hour_ones_type = DIGIT_PRIORITY;
        minute_tens_type = DIGIT_MIDPRIORITY;
        minute_ones_type = DIGIT_MIDPRIORITY;
    }
    // Calculate total width of time display with spacing
    int total_width = 0;
    int colon_width = 8;
    int digit_spacing = 2; // Space between digits
    // Add hour tens width if present
    if (hour_tens > 0)
    {
        total_width += get_digit_width(hour_tens_type);
        total_width += digit_spacing; // Space after hour tens
    }
    // Add hour ones width
    total_width += get_digit_width(hour_ones_type);
    total_width += digit_spacing; // Space after hour ones
    // Add colon width
    total_width += colon_width;
    total_width += digit_spacing; // Space after colon
    // Add minute tens width
    total_width += get_digit_width(minute_tens_type);
    total_width += digit_spacing; // Space after minute tens
    // Add minute ones width
    total_width += get_digit_width(minute_ones_type);
    GRect bounds = layer_get_bounds(layer);
    // Circular path parameters
    int center_x = bounds.size.w / 2;
    int center_y = bounds.size.h / 2;
    int radius = 50; // Radius of circular path
    // Draw hour and minute dots if enabled
    if (s_settings.debug_logging) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Drawing dots - show_hour_minute_dots: %d, show_second_dot: %d", 
                s_settings.show_hour_minute_dots, s_settings.show_second_dot);
    }
    if (s_settings.show_hour_minute_dots) {
        // Draw hour dot around circular path (behind everything)
        // Calculate angle based on current hour and minutes for more accuracy
        // 12 hours = 360 degrees, plus minutes contribute to hour position
        // Convert to 12-hour format for proper positioning
        int display_hour = s_current_hour % 12;
        if (display_hour == 0) display_hour = 12; // Handle 12 o'clock case
        float hour_angle = (((display_hour + (s_current_minute / 60.0f)) / 12.0f) * 2.0f
                            * M_PI) - M_PI_2;
        // Calculate hour dot position using trigonometric functions
        int hour_dot_x = center_x + (int)(radius * my_cos(hour_angle));
        int hour_dot_y = center_y + (int)(radius * my_sin(hour_angle));
        // Set hour dot color to gray for visibility
        if (s_settings.dark_mode)
        {
            graphics_context_set_fill_color(ctx, GColorLightGray);
        }
        else
        {
            graphics_context_set_fill_color(ctx, GColorDarkGray);
        }
        // Draw 8px hour dot (behind minute and second hands)
        graphics_fill_circle(ctx, GPoint(hour_dot_x, hour_dot_y),
                             4); // 4px radius = 8px diameter
        
        // Draw minute dot around circular path (in front of hour hand)
        // Calculate angle based on current minute (60 minutes = 360 degrees)
        // Start at top center (12 o'clock position) by subtracting PI/2
        float minute_angle = ((s_current_minute / 60.0f) * 2.0f * M_PI) - M_PI_2;
        // Calculate minute dot position using trigonometric functions
        int minute_dot_x = center_x + (int)(radius * my_cos(minute_angle));
        int minute_dot_y = center_y + (int)(radius * my_sin(minute_angle));
        // Set minute dot color to gray for visibility
        if (s_settings.dark_mode)
        {
            graphics_context_set_fill_color(ctx, GColorLightGray);
        }
        else
        {
            graphics_context_set_fill_color(ctx, GColorDarkGray);
        }
        // Draw 8px minute dot (in front of hour hand)
        graphics_fill_circle(ctx, GPoint(minute_dot_x, minute_dot_y),
                             4); // 4px radius = 8px diameter
    }
    
    // Draw second dot if enabled
    if (s_settings.show_second_dot) {
        // Draw second dot around circular path (in front of everything)
        // Calculate angle based on current second (60 seconds = 360 degrees)
        // Start at top center (12 o'clock position) by subtracting PI/2
        float angle = ((s_current_second / 60.0f) * 2.0f * M_PI) - M_PI_2;
        // Calculate dot position using trigonometric functions
        int dot_x = center_x + (int)(radius * my_cos(angle));
        int dot_y = center_y + (int)(radius * my_sin(angle));
        // Set second dot color based on dark mode
        if (s_settings.dark_mode)
        {
            graphics_context_set_fill_color(ctx, GColorWhite);
        }
        else
        {
            graphics_context_set_fill_color(ctx, GColorBlack);
        }
        // Draw 8px second dot (in front of minute and hour hands)
        graphics_fill_circle(ctx, GPoint(dot_x, dot_y), 4); // 4px radius = 8px diameter
    }
    // Draw white rectangle behind time display to obscure dot
    int time_display_height = SPRITE_HEIGHT; // Exact sprite height
    int time_display_width = total_width; // Exact total width
    int time_display_x = (bounds.size.w - time_display_width) / 2;
    int time_display_y = (bounds.size.h - time_display_height) / 2;
    if (s_settings.dark_mode)
    {
        graphics_context_set_fill_color(ctx, GColorBlack);
    }
    else
    {
        graphics_context_set_fill_color(ctx, GColorWhite);
    }
    graphics_fill_rect(ctx, GRect(time_display_x, time_display_y,
                                  time_display_width, time_display_height), 0, GCornerNone);
    // Starting X position to center the time display
    int start_x = (bounds.size.w - total_width) / 2;
    int y_pos = (bounds.size.h - SPRITE_HEIGHT) / 2;
    int current_x = start_x;
    // Draw hour tens digit if present
    if (hour_tens > 0)
    {
        draw_digit(ctx, hour_tens, hour_tens_type, current_x, y_pos);
        current_x += get_digit_width(hour_tens_type);
        current_x += digit_spacing; // Space after hour tens
    }
    // Draw hour ones digit
    draw_digit(ctx, hour_ones, hour_ones_type, current_x, y_pos);
    current_x += get_digit_width(hour_ones_type);
    current_x += digit_spacing; // Space after hour ones
    // Draw colon between hours and minutes
    if (s_settings.dark_mode)
    {
        graphics_context_set_fill_color(ctx, GColorWhite);
    }
    else
    {
        graphics_context_set_fill_color(ctx, GColorBlack);
    }
    graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 4, 4, 4), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 10, 4, 4), 0, GCornerNone);
    current_x += colon_width;
    current_x += digit_spacing; // Space after colon
    // Draw minute tens digit
    draw_digit(ctx, minute_tens, minute_tens_type, current_x, y_pos);
    current_x += get_digit_width(minute_tens_type);
    current_x += digit_spacing; // Space after minute tens
    // Draw minute ones digit
    draw_digit(ctx, minute_ones, minute_ones_type, current_x, y_pos);
    // Draw widgets in top corners using the widget system
    widgets_draw_corner(ctx, CORNER_TOP_LEFT, tick_time);
    widgets_draw_corner(ctx, CORNER_TOP_RIGHT, tick_time);
    // Draw day abbreviation in bottom left corner
    if (s_day_sprites)
    {
        int padding_bottom = 10; // Bottom padding
        int padding_left = 10; // Left padding
        // Use the day_of_week variable (which may be overridden by debug mode)
        // Map day of week to abbreviation based on setting
        const char *day_abbrev = "";
        if (s_settings.use_two_letter_day)
        {
            // Two-letter abbreviations
            switch (day_of_week)
            {
                case 0: day_abbrev = "SU"; break;
                case 1: day_abbrev = "MO"; break;
                case 2: day_abbrev = "TU"; break;
                case 3: day_abbrev = "WE"; break;
                case 4: day_abbrev = "TH"; break;
                case 5: day_abbrev = "FR"; break;
                case 6: day_abbrev = "SA"; break;
                default: day_abbrev = "ER"; break;
            }
        }
        else
        {
            // Three-letter abbreviations
            switch (day_of_week)
            {
                case 0: day_abbrev = "SUN"; break;
                case 1: day_abbrev = "MON"; break;
                case 2: day_abbrev = "TUE"; break;
                case 3: day_abbrev = "WED"; break;
                case 4: day_abbrev = "THU"; break;
                case 5: day_abbrev = "FRI"; break;
                case 6: day_abbrev = "SAT"; break;
                default: day_abbrev = "ERR"; break;
            }
        }
        // Calculate Y position for bottom alignment
        int y_pos = bounds.size.h - DAY_HEIGHT - padding_bottom;
        int start_x = padding_left;
        // Draw day of week abbreviation with split positioning
        if (s_settings.use_two_letter_day)
        {
            // Two-letter abbreviations: first letter in bottom left, last letter in bottom right
            draw_day_char(ctx, day_abbrev[0], start_x,
                          y_pos); // First letter in left corner
            // Calculate position for last letter in bottom right corner
            int right_x = bounds.size.w - DAY_WIDTH - padding_left;
            draw_day_char(ctx, day_abbrev[1], right_x,
                          y_pos); // Last letter in right corner
        }
        else
        {
            // Three-letter abbreviations: first letter in bottom left, middle letter in bottom middle, last letter in bottom right
            draw_day_char(ctx, day_abbrev[0], start_x,
                          y_pos); // First letter in left corner
            // Calculate position for middle letter in bottom middle
            int middle_x = (bounds.size.w - DAY_WIDTH) / 2;
            draw_day_char(ctx, day_abbrev[1], middle_x, y_pos); // Middle letter in center
            // Calculate position for last letter in bottom right corner
            int right_x = bounds.size.w - DAY_WIDTH - padding_left;
            draw_day_char(ctx, day_abbrev[2], right_x,
                          y_pos); // Last letter in right corner
        }
    }
}

static void main_window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    // Initialize time variables with current time
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    s_current_second = tick_time->tm_sec;
    s_current_minute = tick_time->tm_min;
    s_current_hour = tick_time->tm_hour;
    // Create canvas layer for drawing first
    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    // Load sprite sheets for time display (not handled by widgets)
    s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
    s_subpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_SUBPRIORITY_DIGIT);
    s_midpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_MIDPRIORITY_DIGIT);
    s_day_sprites = gbitmap_create_with_resource(RESOURCE_ID_DAY_SPRITES);
    // Check if resources loaded successfully
    if (!s_priority_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load priority digit sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_priority_sprites).size;
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "Priority sprite sheet loaded: %dx%d", size.w,
                    size.h);
        }
    }
    if (!s_day_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load day sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_day_sprites).size;
        if (s_settings.debug_logging) {
            APP_LOG(APP_LOG_LEVEL_INFO, "Day sprite sheet loaded: %dx%d", size.w, size.h);
        }
    }
    // Invert palette colors for dark mode
    if (s_settings.dark_mode)
    {
        invert_bitmap_palette(s_priority_sprites);
        invert_bitmap_palette(s_subpriority_sprites);
        invert_bitmap_palette(s_midpriority_sprites);
        invert_bitmap_palette(s_day_sprites);
    }
    // Force initial redraw
    layer_mark_dirty(s_canvas_layer);
    // Subscribe to tick timer service for updates - include all time units for rotating dots
    tick_timer_service_subscribe(MINUTE_UNIT | SECOND_UNIT | HOUR_UNIT,
                                 tick_handler);
}

static void main_window_unload(Window *window)
{
    // Clean up resources
    layer_destroy(s_canvas_layer);
    gbitmap_destroy(s_priority_sprites);
    gbitmap_destroy(s_subpriority_sprites);
    gbitmap_destroy(s_midpriority_sprites);
    gbitmap_destroy(s_day_sprites);
}

static void init()
{
    // Initialize settings with defaults first
    s_settings = get_default_settings();
    
    // Load settings from persistent storage (will override defaults if they exist)
    prv_load_settings();
    
    // FORCE debug settings to always use config.h defaults (not user-configurable)
    s_settings.debug_mode = DEFAULT_DEBUG_MODE;
    s_settings.debug_logging = DEFAULT_DEBUG_LOGGING;
    
    // Link settings to widget system
    s_settings_dark_mode = s_settings.dark_mode;
    s_settings_debug_logging = s_settings.debug_logging;
    
    // Start debug timer if debug mode is enabled in config
    if (s_settings.debug_mode && !s_debug_timer) {
        s_debug_counter = 0;
        s_debug_timer = app_timer_register(500, debug_timer_callback, NULL);
    }
    
    // Initialize widget system
    widgets_init();
    
    // Set widget configuration from saved settings
    widgets_set_config(s_settings.widget_config);
    
    // Set step goal from saved settings
    widgets_set_step_goal(s_settings.step_goal);
    
    // Create main Window element and assign to pointer
    s_main_window = window_create();
    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(s_main_window, (WindowHandlers)
    {
        .load = main_window_load,
        .unload = main_window_unload
    });
    // Show the Window on the watch, with animated=true
    window_stack_push(s_main_window, true);
    // Initialize AppMessage for Clay configuration
    app_message_register_inbox_received(prv_inbox_received_handler);
    app_message_open(128, 128);
}

static void deinit()
{
    // Deinitialize widget system
    widgets_deinit();
    
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void)
{
    init();
    app_event_loop();
    deinit();
}
