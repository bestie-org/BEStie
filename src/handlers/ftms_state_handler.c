#include "ftms_state_handler.h"
#include "app_timer.h"
#include "sdk_config.h"
#include "services.h"
#include "timestamp.h"

#define NRF_LOG_MODULE_NAME ftms_h
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// FTMS notification timer interval. This dictates how often it checks if sending more data == 0 notification is necessary
#ifndef CFG_FTMS_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS
#define CFG_FTMS_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS 250
#endif

// How often to send FTMS notification with more data == 0 if there's no rider power or cadence updates
#ifndef CFG_FTMS_HANDLER_IDLE_NOTIFICATION_INTERVAL_MS
#define CFG_FTMS_HANDLER_IDLE_NOTIFICATION_INTERVAL_MS 1000
#endif

APP_TIMER_DEF(ftms_notification_timer);

static bool ftms_cadence_updated = false;
static bool ftms_power_updated = false;

static uint32_t initial_odometer = 0;

static uint64_t ftms_power_sum = 0;
static uint32_t ftms_power_sample_count = 0;

static uint64_t ftms_cadence_sum = 0;
static uint32_t ftms_cadence_sample_count = 0;

static uint64_t ftms_update_timestamp_ms = 0;

static void ftms_state_reset(void)
{
	ftms_cadence_updated = false;
	ftms_power_updated = false;
	initial_odometer = 0;
	ftms_power_sum = 0;
	ftms_power_sample_count = 0;
	ftms_cadence_sum = 0;
	ftms_cadence_sample_count = 0;
	ftms_update_timestamp_ms = 0;
}

static void on_initial_data_read(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	const ble_ldi_t *const p_ebike_state = p_ebike_state_data->params.initial_data_read.p_initial_data;
	ASSERT(p_ebike_state);

	ret_code_t err_code;

	ftms_state_reset();

	if(p_ebike_state->has_odometer) {
		initial_odometer = p_ebike_state->odometer;
	}

	err_code = app_timer_start(ftms_notification_timer, APP_TIMER_TICKS(CFG_FTMS_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS), NULL);
	APP_ERROR_CHECK(err_code);
}

static void on_live_data_notification(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	const ble_ldi_t *const p_incoming_msg = p_ebike_state_data->params.live_data_notification.p_incoming_notification;
	const ble_ldi_t *const p_previous_ebike_state = p_ebike_state_data->params.live_data_notification.p_previous_ebike_data;

	ASSERT(p_incoming_msg);
	ASSERT(p_previous_ebike_state);

	if(initial_odometer == 0 && p_incoming_msg->has_odometer) {
		initial_odometer = p_incoming_msg->odometer;
	}

	// fix for data fields going to 0 on Garmin
	// publish 'more data == 1' FTMS update
	// this can't be done for speed because it causes '--' on fields absent in speed update

	ble_ftms_data_t ftms_data = {0};
	if(EBIKE_STATE_HAS_PARAM_CHANGED(rider_power, p_previous_ebike_state, p_incoming_msg)) {
		ftms_data.instantaneous_power_present = true;
		ftms_data.instantaneous_power_W = p_incoming_msg->rider_power;
		ftms_power_updated = true;
	}

	if(EBIKE_STATE_HAS_PARAM_CHANGED(cadence, p_previous_ebike_state, p_incoming_msg)) {
		ftms_data.instantaneous_cadence_present = true;
		ftms_data.instantaneous_cadence_rpm = p_incoming_msg->cadence;
		ftms_cadence_updated = true;
	}

	ret_code_t err_code = ble_ftms_measurement_update(services_ftms_inst_get(), &ftms_data);
	if(err_code == NRF_ERROR_NO_MEM) {
		ftms_power_updated = false;
		ftms_cadence_updated = false;
		NRF_LOG_WARNING("FTMS queue out of space");
	} else {
		APP_ERROR_CHECK(err_code);
	}

	// Accumulate running averages from each LDI notification.
	// Zero values are excluded, matching the e-bike head unit's averaging convention.
	if(p_incoming_msg->has_rider_power && p_incoming_msg->rider_power > 0) {
		ftms_power_sum += (uint64_t)p_incoming_msg->rider_power;
		ftms_power_sample_count++;
	}

	if(p_incoming_msg->has_cadence && p_incoming_msg->cadence > 0) {
		ftms_cadence_sum += (uint64_t)p_incoming_msg->cadence;
		ftms_cadence_sample_count++;
	}
}

static void ftms_notification_timer_handler(void *p_ctx)
{
	uint64_t now = timestamp_get_ms();
	ble_ftms_data_t ftms_data = {0};

	const ble_ldi_t *const p_ebike_state = ebike_state_get();

	// send more data = 0 only on power and cadence updates or every CFG_FTMS_HANDLER_IDLE_NOTIFICATION_INTERVAL_MS
	if(!(ftms_cadence_updated || ftms_power_updated) &&
	   ((now - ftms_update_timestamp_ms) < CFG_FTMS_HANDLER_IDLE_NOTIFICATION_INTERVAL_MS)) {
		return;
	}

	// Including all fields in each 'more data == 0' notification - speed, power, cadence prevents data fields from going '--' on
	// Garmin devices
	// exception: value was broadcasted in 'more data == 1' not too long ago (1s or thereabouts)
	// we're using this to have nicer display of instantaneous power

	if(p_ebike_state->has_speed) {
		ftms_data.instantaneous_speed_present = true;
		ftms_data.instantaneous_speed_kph = (float)p_ebike_state->speed / 100.0f;
	}

	if(p_ebike_state->has_cadence && !ftms_cadence_updated) {
		ftms_data.instantaneous_cadence_present = true;
		ftms_data.instantaneous_cadence_rpm = p_ebike_state->cadence;
	}

	if(ftms_cadence_sample_count > 0) {
		ftms_data.average_cadence_present = true;
		ftms_data.average_cadence_rpm = (int16_t)(ftms_cadence_sum / ftms_cadence_sample_count);
	}

	if(p_ebike_state->has_rider_power && !ftms_power_updated) {
		ftms_data.instantaneous_power_present = true;
		ftms_data.instantaneous_power_W = p_ebike_state->rider_power;
	}

	if(ftms_power_sample_count > 0) {
		ftms_data.average_power_present = true;
		ftms_data.average_power_W = (int16_t)(ftms_power_sum / ftms_power_sample_count);
	}

	if(p_ebike_state->has_odometer && initial_odometer) {
		ftms_data.total_distance_present = true;
		ftms_data.total_distance_m = p_ebike_state->odometer - initial_odometer;
	}

	ret_code_t err_code = ble_ftms_measurement_update(services_ftms_inst_get(), &ftms_data);
	if(err_code == NRF_ERROR_NO_MEM) {
		NRF_LOG_WARNING("FTMS queue out of space");
	} else {
		APP_ERROR_CHECK(err_code);
	}

	ftms_power_updated = false;
	ftms_cadence_updated = false;
	ftms_update_timestamp_ms = now;
}

static void ftms_ebike_event_handler(const ebike_state_data_t *const p_ebike_state_data)
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
		app_timer_stop(ftms_notification_timer);
		break;
	}
}

void ftms_state_handler_init(void)
{
	ret_code_t err_code;

	err_code = app_timer_create(&ftms_notification_timer, APP_TIMER_MODE_REPEATED, ftms_notification_timer_handler);
	APP_ERROR_CHECK(err_code);

	err_code = ebike_state_register_handler(ftms_ebike_event_handler);
	APP_ERROR_CHECK(err_code);
}
