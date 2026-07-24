#include <math.h>

#include "app_timer.h"
#include "battery_state_handler.h"
#include "ble_bas.h"
#include "bsp.h"
#include "nrfx_saadc.h"
#include "sdk_config.h"
#include "services.h"

#define NRF_LOG_MODULE_NAME bas_h
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// battery measurement timer interval
#ifndef CFG_BATTERY_MEASUREMENT_TIMER_INTERVAL_MS
#define CFG_BATTERY_MEASUREMENT_TIMER_INTERVAL_MS (2 * 60 * 1000)
#endif

// pause in case SAADC is busy
#ifndef CFG_BATTERY_MEASUREMENT_BUSY_DELAY_MS
#define CFG_BATTERY_MEASUREMENT_BUSY_DELAY_MS 500
#endif

// how many samples between SAADC calibrations
#ifndef CFG_BATTERY_MEASUREMENT_SAADC_CALIBRATE_EVERY
#define CFG_BATTERY_MEASUREMENT_SAADC_CALIBRATE_EVERY 100
#endif

APP_TIMER_DEF(battery_measurement_timer);

// ALPHA = 2 / (Number of samples in moving average + 1)
#define ALPHA 0.2f

static float battery_level_average = 0.0f;
static bool battery_level_average_initalized = false;
static uint32_t samples_since_last_calibration = 0;

// exponential moving average. Power draw is low but extemally noisy. Some samples will hit high current draw windows.
static float average_battery_level(const float battery_level)
{
	if(!battery_level_average_initalized) {
		battery_level_average = battery_level;
		battery_level_average_initalized = true;
	} else {
		battery_level_average = battery_level_average + ALPHA * (battery_level - battery_level_average);
	}

	return battery_level_average;
}

// note: this is executed in REAL interrupt context
static void saadc_calibration_handler(const nrfx_saadc_evt_t *const p_event)
{
	ret_code_t err_code;

	switch(p_event->type) {
	case NRFX_SAADC_EVT_CALIBRATEDONE:
		err_code = app_timer_start(battery_measurement_timer, APP_TIMER_TICKS(CFG_BATTERY_MEASUREMENT_BUSY_DELAY_MS), NULL);
		APP_ERROR_CHECK(err_code);
		break;
	default: // do nothing
		break;
	}
}

static void battery_measurement_timer_handler(void *p_ctx)
{
	float soc = 0.0f;
	ret_code_t err_code;

	if(samples_since_last_calibration++ >= CFG_BATTERY_MEASUREMENT_SAADC_CALIBRATE_EVERY) {

		NRF_LOG_DEBUG("Running SAADC calibration");

		err_code = bsp_saadc_calibrate(saadc_calibration_handler);
		if(err_code != NRF_ERROR_BUSY) {
			samples_since_last_calibration = 0;
			return;
		}
	} else {
		err_code = bsp_get_soc(&soc);
	}

	if(err_code == NRF_ERROR_BUSY) {

		NRF_LOG_DEBUG("SAADC is busy, retrying after a delay");

		err_code = app_timer_start(battery_measurement_timer, APP_TIMER_TICKS(CFG_BATTERY_MEASUREMENT_BUSY_DELAY_MS), NULL);
		APP_ERROR_CHECK(err_code);

		return;
	}
	APP_ERROR_CHECK(err_code);

	float averaged_soc = average_battery_level(soc);

	NRF_LOG_DEBUG("Average SOC: " NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(averaged_soc));

	// no error checking here, anything that can happen is intermittent and retry will suffice
	ble_bas_update(services_bas_inst_get(), floorf(averaged_soc));

	err_code = app_timer_start(battery_measurement_timer, APP_TIMER_TICKS(CFG_BATTERY_MEASUREMENT_TIMER_INTERVAL_MS), NULL);
	APP_ERROR_CHECK(err_code);
}

void battery_state_handler_init(void)
{
	ret_code_t err_code;

	err_code = app_timer_create(&battery_measurement_timer, APP_TIMER_MODE_SINGLE_SHOT, battery_measurement_timer_handler);
	APP_ERROR_CHECK(err_code);

	// do initial measurement right away and kick off the timer
	battery_measurement_timer_handler(NULL);
}
