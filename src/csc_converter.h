#ifndef __CSC_CONVERTER_H__
#define __CSC_CONVERTER_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t total_wheel_revolutions;
	uint16_t last_wheel_event_for_speed_cadence; // in 1/1024ms units
	uint16_t last_wheel_event_for_cycling_power; // in 1/2048ms units

	uint16_t total_crank_revolutions;
	uint16_t last_crank_event;

} csc_converter_data_t;

void csc_converter_process_cadence(const int16_t cadence_rpm, const uint64_t timestamp_ms);
void csc_converter_process_speed(const float speed_kph, const uint64_t timestamp_ms);

// return virtual wheel circumference in meters used for speed calculations
float csc_converter_get_wheel_circumference(void);

void csc_converter_get_state(csc_converter_data_t *const p_out);

void csc_converter_reset(void);

#endif
