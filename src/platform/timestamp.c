#include "timestamp.h"
#include "app_timer.h"

/**
 *	monotonic timestamp counter
 *
 *	note: this value will technically roll over but it takes millions of years
 */
static uint64_t timestamp_ticks = 0;

// if true indicates that timestamp_init() was already called
static bool timestamp_is_init = false;

// keep track of last rtc counter value for app_timer_cnt_diff_compute()
static uint32_t last_ticks = 0;

// update internal timer ticks counter
static void timestamp_update_ticks(void)
{
	uint32_t curr_ticks = app_timer_cnt_get();
	timestamp_ticks += app_timer_cnt_diff_compute(curr_ticks, last_ticks);
	last_ticks = curr_ticks;
}

void timestamp_init(void)
{
	if(timestamp_is_init) {
		return;
	}
	app_timer_resume();
	last_ticks = app_timer_cnt_get();
	timestamp_is_init = true;
}

uint64_t timestamp_get_ms(void)
{
	if(!timestamp_is_init)
		timestamp_init();
	timestamp_update_ticks();

	return timestamp_ticks / APP_TIMER_TICKS(1);
}

uint64_t timestamp_get_s(void)
{
	return timestamp_get_ms() / 1000;
}

void timestamp_reset(void)
{
	timestamp_ticks = 0;
	last_ticks = app_timer_cnt_get();
}
