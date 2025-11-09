#include <pebble.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_priority_sprites;
static GBitmap *s_lesser_sprites;
static GBitmap *s_least_sprites;
static GBitmap *s_am_pm_indicator;

// Sprite sheet dimensions
#define PRIORITY_WIDTH 40
#define LESSER_WIDTH 20
#define LEAST_WIDTH 13
#define SPRITE_HEIGHT 18
#define SPRITES_PER_ROW 3
#define SPRITES_PER_COLUMN 4

// Digit types for width selection
typedef enum {
  DIGIT_PRIORITY,
  DIGIT_LESSER,
  DIGIT_LEAST
} DigitType;

// Function to draw a digit with specified type
static void draw_digit(GContext *ctx, int digit, DigitType type, int x, int y) {
  GBitmap *sprite_sheet = s_priority_sprites; // Default to priority
  int sprite_width = PRIORITY_WIDTH; // Default to priority width
  
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
  
  // Draw the sprite
  graphics_draw_bitmap_in_rect(ctx, sprite_bitmap, dest_rect);
  
  // Clean up the sub-bitmap
  gbitmap_destroy(sprite_bitmap);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Refresh the display when time changes
  layer_mark_dirty(s_canvas_layer);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Set background to white
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  
  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int hour = tick_time->tm_hour;
  int minute = tick_time->tm_min;
  bool is_pm = (hour >= 12); // Determine AM/PM
  
  // Convert 24-hour to 12-hour format
  if (hour > 12) {
    hour -= 12;
  } else if (hour == 0) {
    hour = 12;
  }
  
  // Get hour digits
  int hour_tens = hour / 10;
  int hour_ones = hour % 10;
  
  // Get minute digits
  int minute_tens = minute / 10;
  int minute_ones = minute % 10;
  
  // Determine hour digit types based on your logic
  DigitType hour_tens_type = DIGIT_PRIORITY; // Initialize with default
  DigitType hour_ones_type = DIGIT_PRIORITY; // Initialize with default
  
  if (hour_tens == 0) {
    // Single digit hour (1-9) - use priority digit
    hour_ones_type = DIGIT_PRIORITY;
  } else {
    // Two digit hour (10, 11, 12) - use least for 1, priority for second digit
    hour_tens_type = DIGIT_LEAST;
    hour_ones_type = DIGIT_PRIORITY;
  }
  
  // Determine minute digit types based on your logic
  DigitType minute_tens_type = DIGIT_PRIORITY; // Initialize with default
  DigitType minute_ones_type = DIGIT_PRIORITY; // Initialize with default
  
  if (minute_tens == 0) {
    // Single digit minute (00-09) - first digit is least, second digit is priority
    minute_tens_type = DIGIT_LEAST;  // Zero uses least
    minute_ones_type = DIGIT_PRIORITY;  // Single digit uses priority
  } else if (minute_ones == 0) {
    // If time ends in zero (20, 30, etc.), first digit is priority, second digit is least
    // This overrides all other conditions
    minute_tens_type = DIGIT_PRIORITY;
    minute_ones_type = DIGIT_LEAST;
  } else if (hour_tens == 0) {
    // If hour is single priority digit, first minute digit is lesser, second digit is priority
    minute_tens_type = DIGIT_LESSER;
    minute_ones_type = DIGIT_PRIORITY;
  } else {
    // Two digit minute (10-59) - first digit is priority, second digit is lesser
    minute_tens_type = DIGIT_PRIORITY;
    minute_ones_type = DIGIT_LESSER;
  }
  
  // Calculate total width of time display with spacing
  int total_width = 0;
  int colon_width = 8;
  int digit_spacing = 2; // Space between digits

  // Add hour tens width if present
  if (hour_tens > 0) {
    total_width += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                   (hour_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
    total_width += digit_spacing; // Space after hour tens
  }

  // Add hour ones width
  total_width += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (hour_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
  total_width += digit_spacing; // Space after hour ones

  // Add colon width
  total_width += colon_width;
  total_width += digit_spacing; // Space after colon

  // Add minute tens width
  total_width += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (minute_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
  total_width += digit_spacing; // Space after minute tens

  // Add minute ones width
  total_width += (minute_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (minute_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
  
  GRect bounds = layer_get_bounds(layer);
  
  // Starting X position to center the time display
  int start_x = (bounds.size.w - total_width) / 2;
  int y_pos = (bounds.size.h - SPRITE_HEIGHT) / 2;
  int current_x = start_x;
  
  // Draw hour tens digit if present
  if (hour_tens > 0) {
    draw_digit(ctx, hour_tens, hour_tens_type, current_x, y_pos);
    current_x += (hour_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
                 (hour_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
    current_x += digit_spacing; // Space after hour tens
  }
  
  // Draw hour ones digit
  draw_digit(ctx, hour_ones, hour_ones_type, current_x, y_pos);
  current_x += (hour_ones_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
               (hour_ones_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
  current_x += digit_spacing; // Space after hour ones
  
  // Draw colon between hours and minutes
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 4, 4, 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(current_x + 2, y_pos + 10, 4, 4), 0, GCornerNone);
  current_x += colon_width;
  current_x += digit_spacing; // Space after colon
  
  // Draw minute tens digit
  draw_digit(ctx, minute_tens, minute_tens_type, current_x, y_pos);
  current_x += (minute_tens_type == DIGIT_PRIORITY) ? PRIORITY_WIDTH : 
               (minute_tens_type == DIGIT_LESSER) ? LESSER_WIDTH : LEAST_WIDTH;
  current_x += digit_spacing; // Space after minute tens
  
  // Draw minute ones digit
  draw_digit(ctx, minute_ones, minute_ones_type, current_x, y_pos);
  
  // Draw AM/PM indicator in top left corner with padding
  int am_pm_width = 20;
  int am_pm_height = 14;
  int padding_top = 10; // Top padding
  int padding_left = 10; // Left padding
  
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
  
  // Draw analog watch visualization around digital time
  int circle_radius = (int)(total_width * 0.75 / 2); // 75% of digital time width
  int center_x = start_x + total_width / 2;
  int center_y = y_pos + SPRITE_HEIGHT / 2;
  
  // Get seconds for second hand
  int second = tick_time->tm_sec;
  
  // Calculate angles for each hand (in radians)
  // Hour hand: 12 hours = 360 degrees, plus fraction of current hour
  float hour_angle = (float)((hour % 12) * 30 + minute * 0.5) * (M_PI / 180.0);
  // Minute hand: 60 minutes = 360 degrees
  float minute_angle = (float)(minute * 6) * (M_PI / 180.0);
  // Second hand: 60 seconds = 360 degrees
  float second_angle = (float)(second * 6) * (M_PI / 180.0);
  
  // Calculate positions for each hand dot on the circle
  int hour_x = center_x + (int)(circle_radius * sin(hour_angle));
  int hour_y = center_y - (int)(circle_radius * cos(hour_angle));
  
  int minute_x = center_x + (int)(circle_radius * sin(minute_angle));
  int minute_y = center_y - (int)(circle_radius * cos(minute_angle));
  
  int second_x = center_x + (int)(circle_radius * sin(second_angle));
  int second_y = center_y - (int)(circle_radius * cos(second_angle));
  
  // Draw 5px dots for each hand
  graphics_context_set_fill_color(ctx, GColorBlack);
  
  // Hour hand dot
  graphics_fill_circle(ctx, GPoint(hour_x, hour_y), 2);
  
  // Minute hand dot
  graphics_fill_circle(ctx, GPoint(minute_x, minute_y), 2);
  
  // Second hand dot (slightly smaller for visual distinction)
  graphics_fill_circle(ctx, GPoint(second_x, second_y), 1);
}

static void main_window_load(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Load all three sprite sheets
  s_priority_sprites = gbitmap_create_with_resource(RESOURCE_ID_PRIORITY_DIGIT);
  s_lesser_sprites = gbitmap_create_with_resource(RESOURCE_ID_LESSER_DIGIT);
  s_least_sprites = gbitmap_create_with_resource(RESOURCE_ID_LEAST_DIGIT);
  s_am_pm_indicator = gbitmap_create_with_resource(RESOURCE_ID_AM_PM_INDICATOR);
  
  // Create canvas layer for drawing
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
  
  // Force initial redraw
  layer_mark_dirty(s_canvas_layer);
  
  // Subscribe to tick timer service for updates
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void main_window_unload(Window *window)
{
  // Clean up resources
  layer_destroy(s_canvas_layer);
  gbitmap_destroy(s_priority_sprites);
  gbitmap_destroy(s_lesser_sprites);
  gbitmap_destroy(s_least_sprites);
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
