#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "csc_converter.h"
#include "nrf_assert.h"

// lower tau means that current sample makes bigger contribution to the average and filtered value quicker locks on average at
// cost of higher noise
#define CADENCE_FILTER_TAU_MS 200.0f

// The lower cadence is the less often LDI data contain updates, biggest measured gap is about 4.2s
#define CADENCE_TIMEOUT_MS	4500ULL

#define SPEED_FILTER_TAU_MS 200.0f
#define SPEED_TIMEOUT_MS	1500ULL

// below this speed we consider bike to be stationary
#define SPEED_STATIONARY_THRESHOLD_KPH 1.0f

// virtual wheel circumference in meters. Roughly in 29 inch MTB wheel range.
static const float wheel_circumference_m = 2.20f;

#define POWER_TIMEOUT_MS		  2000ULL
#define POWER_AVERAGING_WINDOW_MS 1000.0f

typedef struct
{
	float filtered_cadence_rpm;
	float fractional_crank_revolutions;
	uint64_t last_cadence_update_ms;
	uint16_t total_crank_revolutions;
	uint16_t last_crank_time;

	float filtered_speed_kph;
	float fractional_wheel_revolutions;
	uint64_t last_speed_update_ms;
	uint16_t total_wheel_revolutions;
	uint16_t last_wheel_time_csc;
	uint16_t last_wheel_time_cycling_power;

} csc_converter_state_t;

static csc_converter_state_t converter_state = {0};

float csc_converter_get_wheel_circumference(void)
{
	return wheel_circumference_m;
}

void csc_converter_process_cadence(const int16_t cadence_rpm, const uint64_t timestamp_ms)
{
	// First boot or recovery from a stationary pause
	if(!cadence_rpm || converter_state.last_cadence_update_ms == 0 ||
	   (timestamp_ms - converter_state.last_cadence_update_ms) > CADENCE_TIMEOUT_MS) {
		converter_state.last_cadence_update_ms = timestamp_ms;

		// Instantly snap to the new value so we don't lag when starting from 0
		converter_state.filtered_cadence_rpm = cadence_rpm;

		// Zero out old fractional progress to prevent "phantom" revolutions
		converter_state.fractional_crank_revolutions = 0.0f;

		if(!cadence_rpm) {
			converter_state.last_crank_time = 0xffff;
			return;
		}

		// Fast path: prime revolution counter on restart
		// Emit one virtual revolution back-dated by one revolution
		// period, cadence snaps to initial update data
		float period_ms = 60000.0f / (float)cadence_rpm;
		float event_time_ms = (float)timestamp_ms - period_ms;
		if(event_time_ms < 0.0f) {
			event_time_ms = 0.0f;
		}
		uint64_t absolute_ble_ticks = (uint64_t)(event_time_ms * 1.024f);

		converter_state.total_crank_revolutions += 1;
		converter_state.last_crank_time = (uint16_t)(absolute_ble_ticks & 0xFFFF);
		return;
	}

	// Calculate time elapsed during this update interval
	uint64_t dt_uint = timestamp_ms - converter_state.last_cadence_update_ms;
	converter_state.last_cadence_update_ms = timestamp_ms;

	if(dt_uint == 0) {
		return;
	}

	float dt_ms = (float)dt_uint;

	// Time-aware low-pass filter
	// Linear approximation of alpha: weight = time_passed / filter_duration
	float alpha = dt_ms / CADENCE_FILTER_TAU_MS;
	if(alpha > 1.0f) {
		alpha = 1.0f; // Clamp to max 100% weight
	}

	// Standard linear interpolation: Filter = Filter + alpha * (New - Filter)
	converter_state.filtered_cadence_rpm =
		converter_state.filtered_cadence_rpm + alpha * (cadence_rpm - converter_state.filtered_cadence_rpm);

	// Cyclist is coasting or stopped
	if(converter_state.filtered_cadence_rpm < 1.0f) {
		return;
	}

	// Calculate crank revolutions added
	float delta_revolutions = converter_state.filtered_cadence_rpm * dt_ms * 0.00001666667f;
	converter_state.fractional_crank_revolutions += delta_revolutions;

	// In order to avoid adding jitter propagate only full crank revolutions and back-adjust last event time accordingly
	if(converter_state.fractional_crank_revolutions >= 1.0f) {

		uint32_t whole_revolutions_completed = (uint32_t)converter_state.fractional_crank_revolutions;

		converter_state.fractional_crank_revolutions -= (float)whole_revolutions_completed;

		// Find how large percent of total added crank revolutions is the last, potentially incomplete, one
		// i.e. if crank went 0.3 revolution past full, then 0.3 / delta_revolutions is our overshoot ratio.
		float overshoot_ratio = converter_state.fractional_crank_revolutions / delta_revolutions;

		// Extrapolate backwards to find exact time when last full crank revolution has been completed.
		float time_offset_ms = overshoot_ratio * dt_ms;
		uint64_t exact_event_ms = timestamp_ms - (uint64_t)time_offset_ms;

		// Convert milliseconds to 1/1024 second CSC ticks
		uint64_t absolute_ble_ticks = (uint64_t)((float)exact_event_ms * 1.024f);

		converter_state.total_crank_revolutions =
			(uint16_t)(converter_state.total_crank_revolutions + whole_revolutions_completed);
		converter_state.last_crank_time = (uint16_t)(absolute_ble_ticks & 0xFFFF);
	}
}

void csc_converter_process_speed(const float speed_kph, const uint64_t timestamp_ms)
{
	// First boot or recovery from a stationary pause
	if((speed_kph < SPEED_STATIONARY_THRESHOLD_KPH) || converter_state.last_speed_update_ms == 0 ||
	   (timestamp_ms - converter_state.last_speed_update_ms) > SPEED_TIMEOUT_MS) {
		converter_state.last_speed_update_ms = timestamp_ms;

		// Instantly snap to the new value so we don't lag when starting from 0
		converter_state.filtered_speed_kph = speed_kph;

		// Zero out old fractional progress to prevent "phantom" revolutions
		converter_state.fractional_wheel_revolutions = 0.0f;

		if(speed_kph < SPEED_STATIONARY_THRESHOLD_KPH) {
			converter_state.last_wheel_time_csc = 0xffff;
			converter_state.last_wheel_time_cycling_power = 0xffff;
			return;
		}

		// Fast path: prime wheel revolution counter on restart
		float period_ms = 3600.0f * wheel_circumference_m / speed_kph;
		float event_time_ms = (float)timestamp_ms - period_ms;
		if(event_time_ms < 0.0f) {
			event_time_ms = 0.0f;
		}

		uint64_t absolute_ble_ticks_csc = (uint64_t)(event_time_ms * 1.024f);
		converter_state.last_wheel_time_csc = (uint16_t)(absolute_ble_ticks_csc & 0xFFFF);

		uint64_t absolute_ble_ticks_cycling_power = (uint64_t)(event_time_ms * 2.048f);
		converter_state.last_wheel_time_cycling_power = (uint16_t)(absolute_ble_ticks_cycling_power & 0xFFFF);

		converter_state.total_wheel_revolutions += 1;
		return;
	}

	// Calculate time elapsed during this update interval
	uint64_t dt_uint = timestamp_ms - converter_state.last_speed_update_ms;
	converter_state.last_speed_update_ms = timestamp_ms;

	if(dt_uint == 0) {
		return;
	}

	float dt_ms = (float)dt_uint;

	// Time-aware low-pass filter
	// Linear approximation of alpha: weight = time_passed / filter_duration
	float alpha = dt_ms / SPEED_FILTER_TAU_MS;
	if(alpha > 1.0f) {
		alpha = 1.0f; // Clamp to max 100% weight
	}

	// Standard linear interpolation: Filter = Filter + alpha * (New - Filter)
	converter_state.filtered_speed_kph =
		converter_state.filtered_speed_kph + alpha * (speed_kph - converter_state.filtered_speed_kph);

	// Cyclist is coasting or stopped
	if(converter_state.filtered_speed_kph < SPEED_STATIONARY_THRESHOLD_KPH) {
		return;
	}

	// Calculate wheel revolutions added
	float delta_revolutions = converter_state.filtered_speed_kph * dt_ms * 0.00027777778f / wheel_circumference_m;
	converter_state.fractional_wheel_revolutions += delta_revolutions;

	// In order to avoid adding jitter propagate only full wheel revolutions and back-adjust last event time accordingly
	if(converter_state.fractional_wheel_revolutions >= 1.0f) {

		uint32_t whole_revolutions_completed = (uint32_t)converter_state.fractional_wheel_revolutions;

		converter_state.fractional_wheel_revolutions -= (float)whole_revolutions_completed;

		// Find how large percent of total added wheel revolutions is the last, potentially incomplete, one
		// i.e. if wheel went 0.3 revolution past full, then 0.3 / delta_revolutions is our overshoot ratio.
		float overshoot_ratio = converter_state.fractional_wheel_revolutions / delta_revolutions;

		// Extrapolate backwards to find exact time when last full wheel revolution has been completed.
		float time_offset_ms = overshoot_ratio * dt_ms;
		uint64_t exact_event_ms = timestamp_ms - (uint64_t)time_offset_ms;

		converter_state.total_wheel_revolutions =
			(uint16_t)(converter_state.total_wheel_revolutions + whole_revolutions_completed);

		// Convert milliseconds to 1/1024 second CSC ticks
		uint64_t absolute_ble_ticks_csc = (uint64_t)((float)exact_event_ms * 1.024f);
		converter_state.last_wheel_time_csc = (uint16_t)(absolute_ble_ticks_csc & 0xFFFF);

		// Convert milliseconds to 1/2048 second CSC ticks
		uint64_t absolute_ble_ticks_cycling_power = (uint64_t)((float)exact_event_ms * 2.048f);
		converter_state.last_wheel_time_cycling_power = (uint16_t)(absolute_ble_ticks_cycling_power & 0xFFFF);
	}
}

void csc_converter_get_state(csc_converter_data_t *const p_out)
{
	ASSERT(p_out);

	p_out->total_wheel_revolutions = converter_state.total_wheel_revolutions;
	p_out->last_wheel_event_for_speed_cadence = converter_state.last_wheel_time_csc;
	p_out->last_wheel_event_for_cycling_power = converter_state.last_wheel_time_cycling_power;

	p_out->total_crank_revolutions = converter_state.total_crank_revolutions;
	p_out->last_crank_event = converter_state.last_crank_time;
}

void csc_converter_reset(void)
{
	memset(&converter_state, 0, sizeof(converter_state));
}
