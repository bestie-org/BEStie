#include "ebike_advertising_state_handler.h"
#include "advertising.h"

static void ebike_advertising_state_event_handler(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	switch(p_ebike_state_data->evt) {
	case EBIKE_STATE_EVT_INITIAL_DATA_READ:
		advertising_signal_ebike_connected_state(true);
		break;
	case EBIKE_STATE_EVT_LIVE_DATA_NOTIFICATION:
		// no implementation needed
		break;
	case EBIKE_STATE_DISCONNECTED:
		advertising_signal_ebike_connected_state(false);
		break;
	}
}

void ebike_advertising_state_handler_init(void)
{
	ret_code_t err_code;

	err_code = ebike_state_register_handler(ebike_advertising_state_event_handler);
	APP_ERROR_CHECK(err_code);
}
