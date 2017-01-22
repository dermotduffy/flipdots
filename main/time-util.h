#ifndef TIME_UTIL_H 
#define TIME_UTIL_H

#include <time.h>

void time_sntp_start();
bool have_valid_time();
void get_time_info(struct tm* time_info);

#endif
