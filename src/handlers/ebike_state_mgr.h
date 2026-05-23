#ifndef __EBIKE_STATE_MGR_H__
#define __EBIKE_STATE_MGR_H__

#include "ble_ldi_c.h"
#include "sdk_config.h"

#ifndef EBIKE_STATE_MAX_HANDLERS
#define EBIKE_STATE_MAX_HANDLERS 5
#endif

#define EBIKE_STATE_HAS_PARAM_CHANGED(_param, _old_msg, _new_msg) \
	(((_new_msg)->has_##_param) &&                                \
	 (((_new_msg)->has_##_param != (_old_msg)->has_##_param) || ((_new_msg)->_param != (_old_msg)->_param)))

typedef enum
{
	EBIKE_STATE_EVT_INITIAL_DATA_READ = 0,	// ebike has been connected and initial state has been read
	EBIKE_STATE_EVT_LIVE_DATA_NOTIFICATION, // incoming LDI data
	EBIKE_STATE_DISCONNECTED,
} ebike_state_evt_t;

typedef struct
{
	ebike_state_evt_t evt;
	union
	{
		struct
		{
			const ble_ldi_t *p_initial_data;
		} initial_data_read;
		struct
		{
			const ble_ldi_t *p_previous_ebike_data;	  // ebike LDI state before incoming notification data has been applied
			const ble_ldi_t *p_incoming_notification; // only incoming notification data
		} live_data_notification;

		// no data for ebike disconnected event
	} params;
} ebike_state_data_t;

typedef void (*ebike_state_handler_t)(const ebike_state_data_t *const p_ebike_state_data);

void ebike_state_on_ldi_c_evt(ble_ldi_c_t *p_ble_ldi_c, ble_ldi_c_evt_t *p_evt);
ret_code_t ebike_state_register_handler(ebike_state_handler_t handler);

bool ebike_is_connected(void);

const ble_ldi_t *ebike_state_get(void);

#endif
