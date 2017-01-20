#include <assert.h>
#include <stdbool.h>
#include <time.h>

#include "apps/sntp/sntp.h"
#include "esp_log.h"

#include "time-util.h"

#define SNTP_SERVER "pool.ntp.org"
#define TIMEZONE    "Europe/Dublin"

const static char *LOG_TAG = "time-util";

void time_initalise() {
  ESP_LOGI(LOG_TAG, "Initializing SNTP...");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, SNTP_SERVER);
  sntp_init();

  // Set timezone.
  ESP_LOGI(LOG_TAG, "Setting timezone to %s...", TIMEZONE);
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

bool have_valid_time() {
  struct tm time_info;
  get_time_info(&time_info);

  // tm_year is years since 1900. If unset, tm_year will be
  // (1970-1900)=~70. Just compare against 2017, if it's less assume
  // year is unset.
  return time_info.tm_year >= (2017-1900);
}

void get_time_info(struct tm* time_info) {
  assert(time_info != NULL);

  time_t current_time;
  time(&current_time);
  localtime_r(&current_time, time_info);
}
