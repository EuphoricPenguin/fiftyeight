#include <pebble.h>
#include "math.h"

static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_priority_sprites;
static GBitmap *s_lesser_sprites;
static GBitmap *s_least_sprites;
static GBitmap *s_subpriority_sprites;
static GBitmap *s_am_pm_indicator;

// Hardcoded settings (config page removed)
static bool s_dark_mode = false;
static bool s_show_am_pm = true;
static int s_time_format = 12;

// Rotating dot variables
static int s_current_second = 0;
static int s_current_minute = 0;
static int s_current_hour = 0;

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

// Sprite sheet dimensions
#define PRIORITY_WIDTH 40
#define SUBPRIORITY_WIDTH 30
#define LESSER_WIDTH 20
#define LEAST_WIDTH 13
#define SPRITE_HEIGHT 18
#define SPRITES_PER_ROW 3
#define SPRITES_PER_COLUMN 4

// Digit types for width selection
typedef enum {
  DIGIT_PRIORITY,
  DIGIT_LESSER,
  DIGIT_LEAST,
  DIGIT_SUBPRIORITY
} DigitType;

// Function to draw a digit with specified type
static void draw_digit(GContext *ctx, int digit, DigitType type, int x, int y) {
  GBitmap *sprite_sheet = NULL;
  int sprite_width = 0;
  
  // Select the appropriate sprite sheet and width
  switch (type) {
    case DIGIT_PRIORITY:
      sprite_sheet = s_priority_sprites;
      sprite_width = PRIORITY_WIDTH;
      break;
    case DIGIT_LESSER:
      sprite_sheet = s_lesser_sprites;
      sprite_width = LESSER_WIDTH;
      break;
    case DIGIT_LEAST:
      sprite_sheet = s_least_sprites;
      sprite_width = LEAST_WIDTH;
      break;
    case DIGIT_SUBPRIORITY:
      sprite_sheet = s_subpriority_sprites;
      sprite_width = SUBPRIORITY_WIDTH;
      break;
  }
  
  // Validate sprite sheet exists
  if (!sprite_sheet) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Sprite sheet is NULL for digit type: %d", type);
    return;
  }
  
  // Validate sprite sheet bounds
  GSize sprite_sheet_size = gbitmap_get_bounds(sprite_sheet).size;
  if (sprite_sheet_size.w <= 0 || sprite_sheet_size.h <= 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid sprite sheet dimensions: %dx%d", sprite_sheet_size.w, sprite_sheet_size.h);
    return;
  }
  
  // Calculate sprite position in the spritesheet
  // Handle digit 0 specially (it's in row 3, column 0)
  int sprite_row, sprite_col;
  if (digit == 0) {
    sprite_row = 3;
    sprite_col = 0;
  } else {
    sprite_row = (digit - 1) / SPRITES_PER_ROW;
    sprite_col = (digit - 1) % SPRITES_PER_ROW;
  }
  
  // Validate sprite position is within bounds
  int max_col = sprite_sheet_size.w / sprite_width;
  int max_row = sprite_sheet_size.h / SPRITE_HEIGHT;
  
  if (sprite_col >= max_col || sprite_row >= max_row) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Sprite position out of bounds: digit=%d, row=%d/%d, col=%d/%d", 
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
  GBitmap *sprite_bitmap = gbitmap_create_as_sub_bitmap(sprite_sheet, source_rect);
  if (!sprite_bitmap) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub-bitmap for digit %d", digit);
    return;
  }
  
  // Draw the sprite
  graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
  
  // Clean up the sub-bitmap
  gbitmap_destroy(sprite_bitmap);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Update current time values and refresh display
  if (units_changed & SECOND_UNIT) {
    s_current_second = tick_time->tm_sec;
    layer_mark_dirty(s_canvas_layer);
  }
  if (units_changed & MINUTE_UNIT) {
    s_current_minute = tick_time->tm_min;
    layer_mark_dirty(s_canvas_layer);
  }
  if (units_changed & HOUR_UNIT) {
    s_current_hour = tick_time->tm_hour;
    layer_mark_dirty(s_canvas_layer);
  }
}


static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Set background color based on dark mode setting
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorBlack);
  } else {
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
  if (s_time_format == 12) {
    if (hour > 12) {
      hour -= 12;
    } else if (hour == 0) {
      hour = 12;
    }
  }
  
  // Get hour digits
  int hour_tens = hour / 10;
  int hour_ones = hour % 10;
  
  // Get minute digits
  int minute_tens = minute / 10;
  int minute_ones = minute % 10;
  
  // Determine hour digit types based on refined logic
  DigitType hour_tens_type = DIGIT_PRIORITY; // Initialize with default
  DigitType hour_ones_type = DIGIT_PRIORITY; // Initialize with default
  
  if (hour_tens == 0) {
    // Single digit hour (1-9) - use priority digit (wide)
    hour_ones_type = DIGIT_PRIORITY;
  } else {
    // Two digit hour (10, 11, 12) - use least for 1, priority for second digit
    hour_tens_type = DIGIT_LEAST;
    hour_ones_type = DIGIT_PRIORITY;
    
    // Special case for 11: use two subpriority digits
    if (hour_tens == 1 && hour_ones == 1) {
      hour_tens_type = DIGIT_SUBPRIORITY;
      hour_ones_type = DIGIT_SUBPRIORITY;
    }
  }
  
  // Determine minute digit types based on refined logic
  DigitType minute_tens_type = DIGIT_SUBPRIORITY; // Initialize with subpriority by default
  DigitType minute_ones_type = DIGIT_SUBPRIORITY; // Initialize with subpriority by default
  
  if (minute_tens == 0 && minute_ones > 0) {
    // Single digit minute (01-09) - first digit is lesser, second digit is subpriority
    // Exception: 00 remains as subpriority for both digits
    minute_tens_type = DIGIT_LESSER;  // Zero uses lesser
    minute_ones_type = DIGIT_SUBPRIORITY;  // Single digit uses subpriority
  }
  
  // Calculate total width of time display with spacing
  int total_width = 0;
  int colon_width = 8;
  int digit_spacing = 2; // Space between digits

  // Add hour tens width if present
  if (hour_tens > 0) {
    total_width += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                   (hour_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : 
                   (hour_tens_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
    total_width += digit_spacing; // Space after hour tens
  }

  // Add hour ones width
  total_width += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (hour_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : 
                 (hour_ones_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
  total_width += digit_spacing; // Space after hour ones

  // Add colon width
  total_width += colon_width;
  total_width += digit_spacing; // Space after colon

  // Add minute tens width
  total_width += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (minute_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : 
                 (minute_tens_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
  total_width += digit_spacing; // Space after minute tens

  // Add minute ones width
  total_width += (minute_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (minute_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : 
                 (minute_ones_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
  
  GRect bounds = layer_get_bounds(layer);
  
  // Draw rotating dot around circular path (behind everything)
  // Calculate angle based on current second (60 seconds = 360 degrees)
  // Start at top center (12 o'clock position) by subtracting PI/2
  float angle = ((s_current_second / 60.0f) * 2.0f * M_PI) - M_PI_2;
  
  // Circular path parameters
  int center_x = bounds.size.w / 2;
  int center_y = bounds.size.h / 2;
  int radius = 50; // Radius of circular path
  
  // Calculate dot position using trigonometric functions
  int dot_x = center_x + (int)(radius * my_cos(angle));
  int dot_y = center_y + (int)(radius * my_sin(angle));
  
  // Set dot color based on dark mode
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorWhite);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
  }
  
  // Draw 8px second dot
  graphics_fill_circle(ctx, GPoint(dot_x, dot_y), 4); // 4px radius = 8px diameter
  
  // Draw minute dot around circular path
  // Calculate angle based on current minute (60 minutes = 360 degrees)
  // Start at top center (12 o'clock position) by subtracting PI/2
  float minute_angle = ((s_current_minute / 60.0f) * 2.0f * M_PI) - M_PI_2;
  
  // Calculate minute dot position using trigonometric functions
  int minute_dot_x = center_x + (int)(radius * my_cos(minute_angle));
  int minute_dot_y = center_y + (int)(radius * my_sin(minute_angle));
  
  // Set minute dot color to solid black (or white in dark mode)
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorWhite);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
  }
  
  // Draw 8px minute dot
  graphics_fill_circle(ctx, GPoint(minute_dot_x, minute_dot_y), 4); // 4px radius = 8px diameter
  
  // Draw hour dot around circular path
  // Calculate angle based on current hour and minutes for more accuracy
  // 12 hours = 360 degrees, plus minutes contribute to hour position
  // Convert to 12-hour format for proper positioning
  int display_hour = s_current_hour % 12;
  if (display_hour == 0) display_hour = 12; // Handle 12 o'clock case
  float hour_angle = (((display_hour + (s_current_minute / 60.0f)) / 12.0f) * 2.0f * M_PI) - M_PI_2;
  
  // Calculate hour dot position using trigonometric functions
  int hour_dot_x = center_x + (int)(radius * my_cos(hour_angle));
  int hour_dot_y = center_y + (int)(radius * my_sin(hour_angle));
  
  // Set all dots to solid black (or white in dark mode)
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorWhite);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
  }
  
  // Draw 8px hour dot
  graphics_fill_circle(ctx, GPoint(hour_dot_x, hour_dot_y), 4); // 4px radius = 8px diameter
  
  // Draw white rectangle behind time display to obscure dot
  int time_display_height = SPRITE_HEIGHT; // Exact sprite height
  int time_display_width = total_width; // Exact total width
  int time_display_x = (bounds.size.w - time_display_width) / 2;
  int time_display_y = (bounds.size.h - time_display_height) / 2;
  
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorBlack);
  } else {
    graphics_context_set_fill_color(ctx, GColorWhite);
  }
  graphics_fill_rect(ctx, GRect(time_display_x, time_display_y, time_display_width, time_display_height), 0, GCornerNone);
  
  // Starting X position to center the time display
  int start_x = (bounds.size.w - total_width) / 2;
  int y_pos = (bounds.size.h - SPRITE_HEIGHT) / 2;
  int current_x = start_x;
  
  // Draw hour tens digit if present
  if (hour_tens > 0) {
    draw_digit(ctx, hour_tens, hour_tens_type, current_x, y_pos);
    current_x += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (hour_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : 
                 (hour_tens_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
    current_x += digit_spacing; // Space after hour tens
  }
  
  // Draw hour ones digit
  draw_digit(ctx, hour_ones, hour_ones_type, current_x, y_pos);
  current_x += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
               (hour_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : 
               (hour_ones_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
  current_x += digit_spacing; // Space after hour ones
  
  // Draw colon between hours and minutes
  if (s_dark_mode) {
    graphics_context_set_fill_color(ctx, GColorWhite);
  } else {
    graphics_context_set_fill_color(ctx, GColorBlack);
  }
  graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 4, 4, 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 10, 4, 4), 0, GCornerNone);
  current_x += colon_width;
  current_x += digit_spacing; // Space after colon
  
  // Draw minute tens digit
  draw_digit(ctx, minute_tens, minute_tens_type, current_x, y_pos);
  current_x += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
               (minute_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : 
               (minute_tens_type == DIGIT_LEAST) ? LEAST_WIDTH : SUBPRIORITY_WIDTH;
  current_x += digit_spacing; // Space after minute tens
  
  // Draw minute ones digit
  draw_digit(ctx, minute_ones, minute_ones_type, current_x, y_pos);
  
  // Draw AM/PM indicator in top left corner with padding (only if enabled and 12-hour format)
  if (s_show_am_pm && s_time_format == 12) {
    int am_pm_width = 20;
    int am_pm_height = 14;
    int padding_top = 10; // Top padding
    int padding_left = 10; // Left padding
    
    // Validate AM/PM indicator exists
    if (s_am_pm_indicator) {
      // Calculate source rectangle for AM/PM indicator
      // Row 0: P (PM), Row 1: A (AM)
      int am_pm_row = is_pm ? 0 : 1;
      GRect am_pm_source_rect = GRect(0, am_pm_row * am_pm_height, am_pm_width, am_pm_height);
      
      // Calculate destination position in top left corner with padding
      GRect am_pm_dest_rect = GRect(padding_left, padding_top, am_pm_width, am_pm_height);
      
      // Set compositing mode for transparency
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      
      // Create a sub-bitmap for the AM/PM indicator
      GBitmap *am_pm_bitmap = gbitmap_create_as_sub_bitmap(s_am_pm_indicator, am_pm_source_rect);
      
      // Draw the AM/PM indicator
      graphics_draw_bitmap_in_rect(ctx, am_pm_bitmap, am_pm_dest_rect);
      
      // Clean up the sub-bitmap
      gbitmap_destroy(am_pm_bitmap);
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
  
  // Load all four sprite sheets with error checking
  s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
  s_lesser_sprites = gbitmap_create_with_resource(RESOURCE_ID_LESSER_DIGIT);
  s_least_sprites = gbitmap_create_with_resource(RESOURCE_ID_LEAST_DIGIT);
  s_subpriority_sprites = gbitmap_create_with_resource(RESOURCE_ID_SUBPRIORITY_DIGIT);
  s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
  
  // Check if resources loaded successfully
  if (!s_priority_sprites) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load priority digit sprite sheet");
  } else {
    GSize size = gbitmap_get_bounds(s_priority_sprites).size;
    APP_LOG(APP_LOG_LEVEL_INFO, "Priority sprite sheet loaded: %dx%d", size.w, size.h);
  }
  
  if (!s_lesser_sprites) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load lesser digit sprite sheet");
  } else {
    GSize size = gbitmap_get_bounds(s_lesser_sprites).size;
    APP_LOG(APP_LOG_LEVEL_INFO, "Lesser sprite sheet loaded: %dx%d", size.w, size.h);
  }
  
  if (!s_least_sprites) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load least digit sprite sheet");
  } else {
    GSize size = gbitmap_get_bounds(s_least_sprites).size;
    APP_LOG(APP_LOG_LEVEL_INFO, "Least sprite sheet loaded: %dx%d", size.w, size.h);
  }
  
  if (!s_am_pm_indicator) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load AM/PM indicator sprite sheet");
  } else {
    GSize size = gbitmap_get_bounds(s_am_pm_indicator).size;
    APP_LOG(APP_LOG_LEVEL_INFO, "AM/PM indicator loaded: %dx%d", size.w, size.h);
  }
  
  // Invert palette colors for dark mode
  if (s_dark_mode) {
    invert_bitmap_palette(s_priority_sprites);
    invert_bitmap_palette(s_lesser_sprites);
    invert_bitmap_palette(s_least_sprites);
    invert_bitmap_palette(s_subpriority_sprites);
    invert_bitmap_palette(s_am_pm_indicator);
  }
  
  // Force initial redraw
  layer_mark_dirty(s_canvas_layer);
  
  // Subscribe to tick timer service for updates - include all time units for rotating dots
  tick_timer_service_subscribe(MINUTE_UNIT | SECOND_UNIT | HOUR_UNIT, tick_handler);
}

static void main_window_unload(Window *window)
{
  // Clean up resources
  layer_destroy(s_canvas_layer);
  gbitmap_destroy(s_priority_sprites);
  gbitmap_destroy(s_lesser_sprites);
  gbitmap_destroy(s_least_sprites);
  gbitmap_destroy(s_subpriority_sprites);
  gbitmap_destroy(s_am_pm_indicator);
}

static void init()
{
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
