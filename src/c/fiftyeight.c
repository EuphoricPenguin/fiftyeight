#include <pebble.h>
#include "math.h"

static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_priority_sprites;
static GBitmap *s_subpriority_sprites;
static GBitmap *s_am_pm_indicator;
static GBitmap *s_day_sprites;
static GBitmap *s_date_sprites;
static GBitmap *s_date_half_sprites;


// Persistent storage key
#define SETTINGS_KEY 1

// Settings struct for persistent storage
typedef struct Settings
{
    bool dark_mode;
    bool show_am_pm;
    bool use_24_hour_format;
    bool use_two_letter_day;
} Settings;

static Settings s_settings =
{
    .dark_mode = false,
    .show_am_pm = false,
    .use_24_hour_format = false,
    .use_two_letter_day = false
};

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
    if (s_am_pm_indicator) gbitmap_destroy(s_am_pm_indicator);
    if (s_day_sprites) gbitmap_destroy(s_day_sprites);
    if (s_date_sprites) gbitmap_destroy(s_date_sprites);
    if (s_date_half_sprites) gbitmap_destroy(s_date_half_sprites);
    // Reload all sprite sheets
    s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
    s_subpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_SUBPRIORITY_DIGIT);
    s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
    s_day_sprites = gbitmap_create_with_resource(RESOURCE_ID_DAY_SPRITES);
    s_date_sprites = gbitmap_create_with_resource(RESOURCE_ID_DATE_SPRITES);
    s_date_half_sprites = gbitmap_create_with_resource(
                              RESOURCE_ID_DATE_HALF_SPRITES);
    // Invert palette colors for dark mode if enabled
    if (s_settings.dark_mode)
    {
        invert_bitmap_palette(s_priority_sprites);
        invert_bitmap_palette(s_subpriority_sprites);
        invert_bitmap_palette(s_am_pm_indicator);
        invert_bitmap_palette(s_day_sprites);
        invert_bitmap_palette(s_date_sprites);
        invert_bitmap_palette(s_date_half_sprites);
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
    Tuple *show_am_pm_t = dict_find(iter, MESSAGE_KEY_ShowAmPm);
    if (show_am_pm_t)
    {
        s_settings.show_am_pm = show_am_pm_t->value->int32 == 1;
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
    // Save settings to persistent storage
    prv_save_settings();
    // If dark mode changed, reload sprites with correct palette
    if (dark_mode_changed)
    {
        prv_reload_sprites();
    }
    // Force redraw to apply new settings
    layer_mark_dirty(s_canvas_layer);
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
#define SUBPRIORITY_WIDTH 30
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

// Date half sprite dimensions (date-half.png - 4x3 grid, 10x14 sprites)
#define DATE_HALF_WIDTH 10
#define DATE_HALF_HEIGHT 14
#define DATE_HALF_SPRITES_PER_ROW 4

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

// Function to draw a date number using half-width sprites (digits from date-half.png)
static void draw_date_half_number(GContext *ctx, int digit, int x, int y)
{
    // Validate sprite sheet exists
    if (!s_date_half_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Date half sprite sheet is NULL");
        return;
    }
    // Validate sprite sheet bounds
    GSize sprite_sheet_size = gbitmap_get_bounds(s_date_half_sprites).size;
    if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid date half sprite sheet dimensions: %dx%d",
                sprite_sheet_size.w, sprite_sheet_size.h);
        return;
    }
    // Map digit to sprite position in the 4x3 grid
    // Layout: 1,2,3,4,5,6,7,8,9,0
    int sprite_index = -1;
    switch (digit)
    {
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
            APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown date half digit: %d", digit);
            return;
    }
    // Calculate sprite position in the spritesheet
    int sprite_row = sprite_index / DATE_HALF_SPRITES_PER_ROW;
    int sprite_col = sprite_index % DATE_HALF_SPRITES_PER_ROW;
    // Validate sprite position is within bounds
    int max_col = sprite_sheet_size.w / DATE_HALF_WIDTH;
    int max_row = sprite_sheet_size.h / DATE_HALF_HEIGHT;
    if (sprite_col >= max_col || sprite_row >= max_row)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Date half sprite position out of bounds: digit=%d, row=%d/%d, col=%d/%d",
                digit, sprite_row, max_row, sprite_col, max_col);
        return;
    }
    // Calculate source rectangle in the spritesheet
    GRect source_rect = GRect(
                            sprite_col * DATE_HALF_WIDTH,
                            sprite_row * DATE_HALF_HEIGHT,
                            DATE_HALF_WIDTH,
                            DATE_HALF_HEIGHT
                        );
    // Calculate destination position
    GRect dest_rect = GRect(x, y, DATE_HALF_WIDTH, DATE_HALF_HEIGHT);
    // Set compositing mode for transparency
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    // Create a sub-bitmap for the specific sprite
    GBitmap *digit_bitmap = gbitmap_create_as_sub_bitmap(s_date_half_sprites,
                            source_rect);
    if (!digit_bitmap)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Failed to create sub-bitmap for date half digit %d",
                digit);
        return;
    }
    // Draw the sprite
    graphics_draw_bitmap_in_rect(ctx, digit_bitmap, dest_rect);
    // Clean up the sub-bitmap
    gbitmap_destroy(digit_bitmap);
}

// Function to draw a date number (digits from date.png)
static void draw_date_number(GContext *ctx, int digit, int x, int y)
{
    // Validate sprite sheet exists
    if (!s_date_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Date sprite sheet is NULL");
        return;
    }
    // Validate sprite sheet bounds
    GSize sprite_sheet_size = gbitmap_get_bounds(s_date_sprites).size;
    if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid date sprite sheet dimensions: %dx%d",
                sprite_sheet_size.w, sprite_sheet_size.h);
        return;
    }
    // Map digit to sprite position in the 3x4 grid
    // Layout: 1,2,3,4,5,6,7,8,9,0
    int sprite_index = -1;
    switch (digit)
    {
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
    if (sprite_col >= max_col || sprite_row >= max_row)
    {
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
    GBitmap *digit_bitmap = gbitmap_create_as_sub_bitmap(s_date_sprites,
                            source_rect);
    if (!digit_bitmap)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub-bitmap for date digit %d",
                digit);
        return;
    }
    // Draw the sprite
    graphics_draw_bitmap_in_rect(ctx, digit_bitmap, dest_rect);
    // Clean up the sub-bitmap
    gbitmap_destroy(digit_bitmap);
}

// Function to draw a month number
static void draw_month_number(GContext *ctx, int month, int x, int y)
{
    // Month numbers are 1-12 (January=1, February=2, ..., December=12)
    int month_number = month + 1; // Convert from 0-based to 1-based
    // Draw month number using appropriate sprites
    if (month_number < 10)
    {
        // Single digit month (1-9) - use full-width sprites
        if (s_date_sprites)
        {
            draw_date_number(ctx, month_number, x, y);
        }
    }
    else
    {
        // Two digit month (10, 11, 12)
        int month_tens = month_number / 10;
        int month_ones = month_number % 10;
        int digit_spacing = 2; // Space between month digits
        // Check if digits are repeating (11)
        bool repeating_digits = (month_tens == month_ones);
        // Check if first digit is 1, 2, or 3 (only 1 applies to months)
        bool use_mixed_sizes = (month_tens == 1) && !repeating_digits;
        if (use_mixed_sizes && s_date_half_sprites && s_date_sprites)
        {
            // First digit half-size, second digit full-size (for 10, 12 where first digit is 1)
            draw_date_half_number(ctx, month_tens, x, y);
            draw_date_number(ctx, month_ones, x + DATE_HALF_WIDTH + digit_spacing, y);
        }
        else if (s_date_sprites)
        {
            // Default to two full-size characters (11 or other cases)
            draw_date_number(ctx, month_tens, x, y);
            draw_date_number(ctx, month_ones, x + DATE_WIDTH + digit_spacing, y);
        }
    }
}

// Digit types for width selection
typedef enum
{
    DIGIT_PRIORITY,
    DIGIT_SUBPRIORITY
} DigitType;

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
    // Get the current time
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    int hour = tick_time->tm_hour;
    int minute = tick_time->tm_min;
    bool is_pm = (hour >= 12); // Determine AM/PM
    // Convert hour based on time format setting
    if (!s_settings.use_24_hour_format)
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
    // - Single digit hours use priority (wide)
    // - All two-digit numbers use subpriority
    DigitType hour_tens_type = DIGIT_SUBPRIORITY;
    DigitType hour_ones_type = DIGIT_SUBPRIORITY;
    DigitType minute_tens_type = DIGIT_SUBPRIORITY;
    DigitType minute_ones_type = DIGIT_SUBPRIORITY;
    
    // Single digit hour (1-9) - use priority digit (wide)
    if (hour_tens == 0)
    {
        hour_ones_type = DIGIT_PRIORITY;
    }
    // Calculate total width of time display with spacing
    int total_width = 0;
    int colon_width = 8;
    int digit_spacing = 2; // Space between digits
    // Add hour tens width if present
    if (hour_tens > 0)
    {
        total_width += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
        total_width += digit_spacing; // Space after hour tens
    }
    // Add hour ones width
    total_width += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
    total_width += digit_spacing; // Space after hour ones
    // Add colon width
    total_width += colon_width;
    total_width += digit_spacing; // Space after colon
    // Add minute tens width
    total_width += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
    total_width += digit_spacing; // Space after minute tens
    // Add minute ones width
    total_width += (minute_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
    GRect bounds = layer_get_bounds(layer);
    // Circular path parameters
    int center_x = bounds.size.w / 2;
    int center_y = bounds.size.h / 2;
    int radius = 50; // Radius of circular path
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
        current_x += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
        current_x += digit_spacing; // Space after hour tens
    }
    // Draw hour ones digit
    draw_digit(ctx, hour_ones, hour_ones_type, current_x, y_pos);
    current_x += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
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
    current_x += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : SUBPRIORITY_WIDTH;
    current_x += digit_spacing; // Space after minute tens
    // Draw minute ones digit
    draw_digit(ctx, minute_ones, minute_ones_type, current_x, y_pos);
    // Draw AM/PM indicator in top left corner with padding (only if enabled and 12-hour format)
    if (s_settings.show_am_pm && !s_settings.use_24_hour_format)
    {
        int am_pm_width = 20;
        int am_pm_height = 14;
        int padding_top = 10; // Top padding
        int padding_left = 10; // Left padding
        // Validate AM/PM indicator exists
        if (s_am_pm_indicator)
        {
            // Calculate source rectangle for AM/PM indicator
            // Row 0: P (PM), Row 1: A (AM)
            int am_pm_row = is_pm ? 0 : 1;
            GRect am_pm_source_rect = GRect(0, am_pm_row * am_pm_height, am_pm_width,
                                            am_pm_height);
            // Calculate destination position in top left corner with padding
            GRect am_pm_dest_rect = GRect(padding_left, padding_top, am_pm_width,
                                          am_pm_height);
            // Set compositing mode for transparency
            graphics_context_set_compositing_mode(ctx, GCompOpSet);
            // Create a sub-bitmap for the AM/PM indicator
            GBitmap *am_pm_bitmap = gbitmap_create_as_sub_bitmap(s_am_pm_indicator,
                                    am_pm_source_rect);
            // Draw the AM/PM indicator
            graphics_draw_bitmap_in_rect(ctx, am_pm_bitmap, am_pm_dest_rect);
            // Clean up the sub-bitmap
            gbitmap_destroy(am_pm_bitmap);
        }
    }
    // If AM/PM is not displayed, show month number in top left corner
    else if (s_date_sprites)
    {
        int padding_top = 10; // Top padding
        int padding_left = 10; // Left padding
        // Get current month (0=January, 1=February, ..., 11=December)
        int current_month = tick_time->tm_mon;
        // Draw month number (1-12)
        draw_month_number(ctx, current_month, padding_left, padding_top);
    }
    // Draw calendar day in top right corner with same padding as AM/PM
    if (s_date_sprites || s_date_half_sprites)
    {
        int padding_top = 10; // Top padding (same as AM/PM)
        int padding_right = 10; // Right padding
        int digit_spacing = 2; // Space between day digits
        // Get current calendar day
        int calendar_day = tick_time->tm_mday;
        // Calculate total width for calendar day (1 or 2 digits)
        int day_number_width;
        if (calendar_day < 10)
        {
            day_number_width = DATE_WIDTH; // Single digit day (full-width)
        }
        else
        {
            // Two digit day - check if we should use mixed sizes
            int day_tens = calendar_day / 10;
            int day_ones = calendar_day % 10;
            bool repeating_digits = (day_tens == day_ones);
            bool use_mixed_sizes = (day_tens == 1 || day_tens == 2 || day_tens == 3) &&
                                   !repeating_digits;
            if (use_mixed_sizes && s_date_half_sprites)
            {
                // Mixed sizes: first digit half-width, second digit full-width
                day_number_width = DATE_HALF_WIDTH + DATE_WIDTH + digit_spacing;
            }
            else
            {
                // Default to two full-width characters
                day_number_width = DATE_WIDTH * 2 + digit_spacing;
            }
        }
        // Calculate starting X position for right alignment
        int start_x = bounds.size.w - day_number_width - padding_right;
        int y_pos = padding_top;
        // Draw calendar day number
        if (calendar_day < 10)
        {
            // Single digit day - use full-width sprites
            if (s_date_sprites)
            {
                draw_date_number(ctx, calendar_day, start_x, y_pos);
            }
        }
        else
        {
            // Two digit day
            int day_tens = calendar_day / 10;
            int day_ones = calendar_day % 10;
            // Check if digits are repeating (11, 22, etc.)
            bool repeating_digits = (day_tens == day_ones);
            // Check if first digit is 1, 2, or 3
            bool use_mixed_sizes = (day_tens == 1 || day_tens == 2 || day_tens == 3) &&
                                   !repeating_digits;
            if (use_mixed_sizes && s_date_half_sprites && s_date_sprites)
            {
                // First digit half-size, second digit full-size (for 1x, 2x, 3x where x != tens digit)
                draw_date_half_number(ctx, day_tens, start_x, y_pos);
                draw_date_number(ctx, day_ones, start_x + DATE_HALF_WIDTH + digit_spacing,
                                 y_pos);
            }
            else if (s_date_sprites)
            {
                // Default to two full-size characters (repeating digits or other cases)
                draw_date_number(ctx, day_tens, start_x, y_pos);
                draw_date_number(ctx, day_ones, start_x + DATE_WIDTH + digit_spacing, y_pos);
            }
        }
    }
    // Draw day abbreviation in bottom left corner
    if (s_day_sprites)
    {
        int padding_bottom = 10; // Bottom padding
        int padding_left = 10; // Left padding
        // Get current day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
        int day_of_week = tick_time->tm_wday;
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
    // Load all sprite sheets with error checking
    s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
    s_subpriority_sprites = gbitmap_create_with_resource(
                                RESOURCE_ID_SUBPRIORITY_DIGIT);
    s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
    s_day_sprites = gbitmap_create_with_resource(RESOURCE_ID_DAY_SPRITES);
    s_date_sprites = gbitmap_create_with_resource(RESOURCE_ID_DATE_SPRITES);
    s_date_half_sprites = gbitmap_create_with_resource(
                              RESOURCE_ID_DATE_HALF_SPRITES);
    // Check if resources loaded successfully
    if (!s_priority_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load priority digit sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_priority_sprites).size;
        APP_LOG(APP_LOG_LEVEL_INFO, "Priority sprite sheet loaded: %dx%d", size.w,
                size.h);
    }
    if (!s_am_pm_indicator)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load AM/PM indicator sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_am_pm_indicator).size;
        APP_LOG(APP_LOG_LEVEL_INFO, "AM/PM indicator loaded: %dx%d", size.w, size.h);
    }
    if (!s_day_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load day sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_day_sprites).size;
        APP_LOG(APP_LOG_LEVEL_INFO, "Day sprite sheet loaded: %dx%d", size.w, size.h);
    }
    if (!s_date_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load date sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_date_sprites).size;
        APP_LOG(APP_LOG_LEVEL_INFO, "Date sprite sheet loaded: %dx%d", size.w, size.h);
    }
    if (!s_date_half_sprites)
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load date half sprite sheet");
    }
    else
    {
        GSize size = gbitmap_get_bounds(s_date_half_sprites).size;
        APP_LOG(APP_LOG_LEVEL_INFO, "Date half sprite sheet loaded: %dx%d", size.w,
                size.h);
    }
    // Invert palette colors for dark mode
    if (s_settings.dark_mode)
    {
        invert_bitmap_palette(s_priority_sprites);
        invert_bitmap_palette(s_subpriority_sprites);
        invert_bitmap_palette(s_am_pm_indicator);
        invert_bitmap_palette(s_day_sprites);
        invert_bitmap_palette(s_date_sprites);
        invert_bitmap_palette(s_date_half_sprites);
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
    gbitmap_destroy(s_am_pm_indicator);
    gbitmap_destroy(s_day_sprites);
    gbitmap_destroy(s_date_sprites);
    gbitmap_destroy(s_date_half_sprites);
}

static void init()
{
    // Load settings from persistent storage
    prv_load_settings();
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
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void)
{
    init();
    app_event_loop();
    deinit();
}
