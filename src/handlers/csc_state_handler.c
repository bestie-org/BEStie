#include "csc_state_handler.h"
#include "app_timer.h"
#include "csc_converter.h"
#include "sdk_config.h"
#include "services.h"
#include "timestamp.h"

#define NRF_LOG_MODULE_NAME csc_h
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#ifndef CSC_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS
#define CSC_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS 1000
#endif

APP_TIMER_DEF(csc_notification_timer);

static uint64_t cpms_update_timestamp_ms = 0;
static uint32_t last_rider_power = 0;

static void csc_state_reset(void)
{
	csc_converter_reset();
	cpms_update_timestamp_ms = 0;
	last_rider_power = 0;
}

static void csc_data_update(void)
{
	csc_converter_data_t converted_sc = {0};
	csc_converter_get_state(&converted_sc);

	// update Cycling Speed and Cadence Service
	ble_csc_data_t csc_data = {
		// speed without cadence and cadence without speed results in blinking of datafields on Edge Explore 2
		.cadence_present = true,
		.speed_present = true,
		.last_crank_event = converted_sc.last_crank_event,
		.total_crank_revolutions = converted_sc.total_crank_revolutions,
		.last_wheel_event = converted_sc.last_wheel_event_for_speed_cadence,
		.total_wheel_revolutions = converted_sc.total_wheel_revolutions,
	};

	ret_code_t err_code = ble_csc_measurement_update(services_csc_inst_get(), &csc_data);
	if(err_code == NRF_ERROR_NO_MEM) {
		NRF_LOG_WARNING("CSC queue out of space");
	} else {
		APP_ERROR_CHECK(err_code);
	}
}

static void cpms_data_update(void)
{
	uint64_t now = timestamp_get_ms();

	csc_converter_data_t converted_sc = {0};
	csc_converter_get_state(&converted_sc);

	ble_cpms_data_t cpms_data = {
		// speed without cadence and cadence without speed results in blinking of datafields on Edge Explore 2
		.cadence_present = true,
		.speed_present = true,
		.instantaneous_power_W = last_rider_power,
		.last_crank_event = converted_sc.last_crank_event,
		.total_crank_revolutions = converted_sc.total_crank_revolutions,
		.last_wheel_event = converted_sc.last_wheel_event_for_cycling_power,
		.total_wheel_revolutions = converted_sc.total_wheel_revolutions,
	};

	ret_code_t err_code = ble_cpms_update(services_cpms_inst_get(), &cpms_data);
	if(err_code == NRF_ERROR_NO_MEM) {
		NRF_LOG_WARNING("CPMS queue out of space");
	} else {
		cpms_update_timestamp_ms = now;
		APP_ERROR_CHECK(err_code);
	}
}

static void on_initial_data_read(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	ret_code_t err_code;

	csc_state_reset();

	err_code = app_timer_start(csc_notification_timer, APP_TIMER_TICKS(CSC_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS), NULL);
	APP_ERROR_CHECK(err_code);
}

static void on_live_data_notification(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	const ble_ldi_t *const p_incoming_msg = p_ebike_state_data->params.live_data_notification.p_incoming_notification;
	const ble_ldi_t *const p_previous_ebike_state = p_ebike_state_data->params.live_data_notification.p_previous_ebike_data;

	ASSERT(p_incoming_msg);
	ASSERT(p_previous_ebike_state);

	uint64_t now = timestamp_get_ms();

	// process Cycling Speed and Cadence / Cycling Power Service data

	if(p_incoming_msg->has_cadence) {
		csc_converter_process_cadence(p_incoming_msg->cadence, now);
	}

	if(p_incoming_msg->has_speed) {
		csc_converter_process_speed((float)p_incoming_msg->speed / 100.0f, now);
	}

	if(p_incoming_msg->has_rider_power) {
		if(p_incoming_msg->rider_power > 0) {

			// allow instant rider power updates while riding but apply rate limits
			if(now - cpms_update_timestamp_ms > (CSC_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS / 2)) {
				last_rider_power = p_incoming_msg->rider_power;
				cpms_data_update();
			} else if(last_rider_power > 0) {
				// two or more power samples between CPMS updates, try to keep as much data as we can
				last_rider_power = (last_rider_power + p_incoming_msg->rider_power) / 2;
			}
		} else {
			// store zero power to be picked up by update timer
			last_rider_power = p_incoming_msg->rider_power;
		}
	}
}

static void csc_notification_timer_handler(void *p_ctx)
{
	// some devices only support speed and cadence service, some support power service and expect speed and cadence to be exposed
	// there
	csc_data_update();
	cpms_data_update();
}

static void csc_ebike_event_handler(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	switch(p_ebike_state_data->evt) {
	case EBIKE_STATE_EVT_INITIAL_DATA_READ:
		on_initial_data_read(p_ebike_state_data);
		break;
	case EBIKE_STATE_EVT_LIVE_DATA_NOTIFICATION:
		on_live_data_notification(p_ebike_state_data);
		break;
	case EBIKE_STATE_DISCONNECTED:
		app_timer_stop(csc_notification_timer);
		break;
	}
}

void csc_state_handler_init(void)
{
	ret_code_t err_code;

	timestamp_init();

	err_code = app_timer_create(&csc_notification_timer, APP_TIMER_MODE_REPEATED, csc_notification_timer_handler);
	APP_ERROR_CHECK(err_code);

	err_code = ebike_state_register_handler(csc_ebike_event_handler);
	APP_ERROR_CHECK(err_code);
}
