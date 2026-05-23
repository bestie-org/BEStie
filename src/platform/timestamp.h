/**
 * Timestamp library
 *
 * This library allows to keep track the time since app_timestamp_init()
 * in for of monotonous counter regardless of the fact that by default
 * RTC counter overflows every 2.4 minutes.
 *
 * note: timestamp_get_X function has to be called at least once per
 * overflow period for the timestamp to be accurate.
 *
 * */
#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

#include "sdk_config.h"
#include <stdint.h>

#if APP_TIMER_KEEPS_RTC_ACTIVE != 1
#error "Enable APP_TIMER_KEEPS_RTC_ACTIVE in sdk_config.h"
#endif

// initialize timestamp counter module
void timestamp_init(void);

// get timestamp - number of seconds since timestamp_init() call
uint64_t timestamp_get_ms(void);

// get timestamp in seconds
uint64_t timestamp_get_s(void);

// reset timestamp counter
void timestamp_reset(void);

#endif
