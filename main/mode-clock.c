#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "displaybuffer.h"
#include "displaydriver.h"
#include "mode-clock.h"

#include "gohufont_11b.h"
#include "liberation_sans_16ptb.h"
#include "liberation_sans_20pt.h"

#define TIME_BUFFER_SIZE               6 // Largest: 'XX:XX\0' => 6
#define GAP_BETWEEN_DIGITS             1

#define MINS_PER_HOUR                  60

// Use 59 steps rather than 60, in order to ensure the last minute of the hour
// shows the board full rather than almost full. Error with these calculations
// is spread over the whole hour.
#define HOUR_ONLY_PIXELS_PER_MIN     (DISPLAY_PIXELS / (MINS_PER_HOUR-1))
#define HOUR_ONLY_PIXELS_PER_MIN_ERR (DISPLAY_PIXELS % (MINS_PER_HOUR-1))

// Total number of points along the edge of a given quadrant.
#define TOTAL_MIN_HAND_END_POINTS      ((DISPLAY_WIDTH*2) + (DISPLAY_HEIGHT*2) - 4)

// Push the digits down the clock face by this number of pixels
// to give the appearance of being centered.
#define FONT_CLOCK_SWEEP_OFFSET_PIXELS         6
#define FONT_SMALL_CLOCK_SWEEP_OFFSET_PIXELS   8
#define FONT_LARGE_HOUR_OFFSET_PIXELS          4
#define FONT_QUESTION_MARK_OFFSET_PIXELS       3

#define HOLLOW_CLOCK_FACE_INNER_RADIUS         7

ModeClockParameters mode_clock_params;

const static char *LOG_TAG = "mode-clock";

static char time_buffer[TIME_BUFFER_SIZE];
static const font_info_t* font_clock_sweep = &liberation_sans_16ptb_font_info;
static const font_info_t* font_large = &liberation_sans_20pt_font_info;
static const font_info_t* font_time_medium = &gohufont_11b_font_info;

static displaybuffer_t buffer_minute_hand;
static displaybuffer_t buffer_clock_face;
static displaybuffer_t buffer_clock_face_hollow;
static displaybuffer_t buffer_template;

bool get_time_info(struct tm* time_info) {
  assert(time_info != NULL);

  time_t current_time;
  time(&current_time);
  localtime_r(&current_time, time_info);

  // tm_year is years since 1900. If unset, tm_year will be
  // (1970-1900)=~70. Just compare against 2017, if it's less assume
  // year is unset.
  return time_info->tm_year >= (2017-1900);
}

static void draw_large_hours(
    int hours, int y_offset, const font_info_t* font,
    displaybuffer_t* displaybuffer) {
  configASSERT(snprintf(time_buffer, TIME_BUFFER_SIZE, "%02i", hours) > 0);

  buffer_tdf_draw_string_centre(
      y_offset, PIXEL_YELLOW,
      time_buffer, GAP_BETWEEN_DIGITS, font, displaybuffer);
}

static void draw_dot_minutes(int mins, displaybuffer_t* displaybuffer) {
  int dots_to_fill = (mins * HOUR_ONLY_PIXELS_PER_MIN) + ceil(
      (mins / ((double) (MINS_PER_HOUR-1))) * HOUR_ONLY_PIXELS_PER_MIN_ERR);

  for (int y = 0; y < DISPLAY_HEIGHT && dots_to_fill > 0; ++y) {
    for (int x = 0;
         x < DISPLAY_WIDTH && dots_to_fill > 0;
         ++x, --dots_to_fill) {
      bool current_value = displaybuffer->data[x][y];
      displaybuffer->data[x][y] = !current_value;
    }
  }
}

static void draw_time(
    const struct tm* time_info, displaybuffer_t* displaybuffer) {
  configASSERT(strftime(
      time_buffer, TIME_BUFFER_SIZE, "%H:%M", time_info) > 0);

  buffer_tdf_draw_string_centre(
    -1, PIXEL_YELLOW, time_buffer, GAP_BETWEEN_DIGITS, font_time_medium, displaybuffer);
}

static void draw_analog_minutes(
    const struct tm* time_info, displaybuffer_t* displaybuffer) {
  // +-- 14--++-- 14 --+
  // |\      || 1    /  |
  // 14  \   ||   /     14
  // |      \||/    2   |
  // +-------41--------+
  // +-------32--------+
  // |      /||\        |
  // 14  /   ||   \     14
  // |/      ||      \  |
  // +-- 14--++-- 14 --+
  int secs = (time_info->tm_min*60) + time_info->tm_sec;
  int chosen_end_point = (TOTAL_MIN_HAND_END_POINTS * secs) / (60*60);
  int x = DISPLAY_WIDTH/2, y = 0;
  int center_x = x;
  int center_y = (DISPLAY_HEIGHT/2)-1;

  for (int i = 0; i < chosen_end_point; ++i) {
    buffer_draw_line(center_x, center_y,
                     x, y, 
                     PIXEL_YELLOW, displaybuffer);

    if (x < (DISPLAY_WIDTH-1) && y == 0) {
      x++;
    } else if (x == (DISPLAY_WIDTH-1) && y < (DISPLAY_HEIGHT/2)-1) {
      y++;
    } else if (x == (DISPLAY_WIDTH-1) && y == (DISPLAY_HEIGHT/2)-1) {
      center_y++;
      y++;
    } else if (x == (DISPLAY_WIDTH-1) && y < DISPLAY_HEIGHT-1) {
      y++;
    } else if (x > DISPLAY_WIDTH/2 && y == DISPLAY_HEIGHT-1) {
      x--;
    } else if (x == (DISPLAY_WIDTH/2) && y == DISPLAY_HEIGHT-1) {
      center_x--;
      x--;
    } else if (x > 0 && y == DISPLAY_HEIGHT-1) {
      x--;
    } else if (x == 0 && y > (DISPLAY_HEIGHT/2)) {
      y--;
    } else if (x == 0 && y == (DISPLAY_HEIGHT/2)) {
      center_y--;
      y--;
    } else if (x == 0 && y > 0) {
      y--;
    } else if (x < (DISPLAY_WIDTH/2)-1 && y == 0) {
      x++;
    } else {
      // Should not get here.
      break;
    }
  }
}

int mode_clock_draw() {
  struct tm time_info;

  if (!get_time_info(&time_info)) {
    ESP_LOGI(LOG_TAG, "Could not draw clock, don't know what time it is!");
 
    // Display an error message if there is no valid time available.
    buffer_wipe(&buffer_draw);
    buffer_tdf_draw_string_centre(
        FONT_QUESTION_MARK_OFFSET_PIXELS, PIXEL_YELLOW, "?",
        GAP_BETWEEN_DIGITS, font_large, &buffer_draw);
    buffer_commit_drawing();
    return CLOCK_TIME_DELAY_BETWEEN_DRAWS_ERROR_MS; 
  } 

  // Update display buffer.
  buffer_wipe(&buffer_draw);

  if (mode_clock_params.clock_style == CLOCK_STYLE_HOUR_ONLY) {
    draw_large_hours(time_info.tm_hour, FONT_LARGE_HOUR_OFFSET_PIXELS,
        font_large, &buffer_draw);
    draw_dot_minutes(time_info.tm_min, &buffer_draw);
  } else if (mode_clock_params.clock_style ==
        CLOCK_STYLE_DIGITAL_HOURS_ANALOG_MINS) {
    draw_large_hours(time_info.tm_hour, FONT_CLOCK_SWEEP_OFFSET_PIXELS,
        font_clock_sweep, &buffer_draw);

    buffer_wipe(&buffer_minute_hand);
    draw_analog_minutes(&time_info, &buffer_minute_hand);

    buffer_wipe(&buffer_template);
    buffer_AND(PIXEL_YELLOW,
               &buffer_minute_hand, &buffer_clock_face, &buffer_template);
    buffer_fill_from_template(PIXEL_INVERSE, &buffer_template, &buffer_draw);
  } else if (mode_clock_params.clock_style ==
        CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS) {
    draw_large_hours(time_info.tm_hour, FONT_SMALL_CLOCK_SWEEP_OFFSET_PIXELS,
        font_time_medium, &buffer_draw);

    buffer_wipe(&buffer_minute_hand);
    draw_analog_minutes(&time_info, &buffer_minute_hand);

    buffer_wipe(&buffer_template);
    buffer_AND(PIXEL_YELLOW,
               &buffer_minute_hand, &buffer_clock_face_hollow, &buffer_template);
    buffer_fill_from_template(PIXEL_YELLOW, &buffer_template, &buffer_draw);

  } else { // Default: CLOCK_STYLE_DIGITAL
    draw_time(&time_info, &buffer_draw);
  }
    
  buffer_commit_drawing();
  
  return CLOCK_TIME_DELAY_BETWEEN_DRAWS_MS;
}

bool mode_clock_set_style(int style) {
  if (style < CLOCK_STYLE_MIN || style > CLOCK_STYLE_MAX) {
    ESP_LOGW(LOG_TAG, "Clock style must be >= %d and <= %d",
        CLOCK_STYLE_MIN, CLOCK_STYLE_MAX);
    return false;
  }
  ESP_LOGI(LOG_TAG, "Setting clock style: %i", style);

  mode_clock_params.clock_style = style;
  return true;
}

void mode_clock_setup() {
  mode_clock_params.clock_style = CLOCK_STYLE_DEFAULT;

  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_minute_hand);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_clock_face);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_clock_face_hollow);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_template);

  // Clock mode designed for a square display.
  assert (DISPLAY_HEIGHT == DISPLAY_WIDTH);
  buffer_fill_circle_centre(
      (DISPLAY_WIDTH/2)-1, PIXEL_YELLOW, &buffer_clock_face);

  // Copy clockface, then hollow it out onto a different buffer.
  buffer_fill_from_template(
      PIXEL_YELLOW, &buffer_clock_face, &buffer_clock_face_hollow);
  buffer_fill_circle_centre(
      HOLLOW_CLOCK_FACE_INNER_RADIUS, PIXEL_BLACK, &buffer_clock_face_hollow);
}
