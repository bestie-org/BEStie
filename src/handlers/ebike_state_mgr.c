#include "ebike_state_mgr.h"
#include "ble_conn_params.h"
#include "services.h"

#define NRF_LOG_MODULE_NAME ESH
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

struct
{
	ebike_state_handler_t handlers[EBIKE_STATE_MAX_HANDLERS];
	size_t len;
} state_handlers = {0};

static uint16_t ebike_conn_handle = BLE_CONN_HANDLE_INVALID;
static ble_ldi_t ebike_state = {0};

// copy only optional fields that are set in p_src to p_dst, leave the rest untouched
static void ebike_state_cpy(ble_ldi_t *const p_dst, const ble_ldi_t *const p_src)
{
	ASSERT(p_dst);
	ASSERT(p_src);

	if(p_src->has_speed) {
		p_dst->has_speed = true;
		p_dst->speed = p_src->speed;
	}
	if(p_src->has_cadence) {
		p_dst->has_cadence = true;
		p_dst->cadence = p_src->cadence;
	}
	if(p_src->has_rider_power) {
		p_dst->has_rider_power = true;
		p_dst->rider_power = p_src->rider_power;
	}
	if(p_src->has_ambient_brightness) {
		p_dst->has_ambient_brightness = true;
		p_dst->ambient_brightness = p_src->ambient_brightness;
	}
	if(p_src->has_battery_soc) {
		p_dst->has_battery_soc = true;
		p_dst->battery_soc = p_src->battery_soc;
	}
	if(p_src->has_time) {
		p_dst->has_time = true;
		p_dst->time = p_src->time;
	}
	if(p_src->has_odometer) {
		p_dst->has_odometer = true;
		p_dst->odometer = p_src->odometer;
	}
	if(p_src->has_bike_light) {
		p_dst->has_bike_light = true;
		p_dst->bike_light = p_src->bike_light;
	}
	if(p_src->has_system_locked) {
		p_dst->has_system_locked = true;
		p_dst->system_locked = p_src->system_locked;
	}
	if(p_src->has_charger_connected) {
		p_dst->has_charger_connected = true;
		p_dst->charger_connected = p_src->charger_connected;
	}
	if(p_src->has_light_reserve_state) {
		p_dst->has_light_reserve_state = true;
		p_dst->light_reserve_state = p_src->light_reserve_state;
	}
	if(p_src->has_diagnosis_program_active) {
		p_dst->has_diagnosis_program_active = true;
		p_dst->diagnosis_program_active = p_src->diagnosis_program_active;
	}
	if(p_src->has_bike_not_driving) {
		p_dst->has_bike_not_driving = true;
		p_dst->bike_not_driving = p_src->bike_not_driving;
	}
}

static void notify_state_handlers(const ebike_state_data_t *const p_ebike_state_data)
{
	ASSERT(p_ebike_state_data);

	for(size_t i = 0; i < state_handlers.len; i++) {
		if(state_handlers.handlers[i]) {
			state_handlers.handlers[i](p_ebike_state_data);
		}
	}
}

static void ebike_state_reset(void)
{
	memset(&ebike_state, 0, sizeof(ebike_state));
}

static void on_ldi_c_discovery_complete(ble_ldi_c_t *const p_ble_ldi_c, const ble_ldi_c_evt_t *const p_evt)
{
	ASSERT(p_ble_ldi_c);
	ASSERT(p_evt);

	ret_code_t err_code;

	NRF_LOG_DEBUG("LDI service discovered on connection 0x%02X", p_evt->conn_handle);

	// Enforce only one ebike connection at any given moment. Do not enable notifications on others.
	if(ebike_conn_handle == BLE_CONN_HANDLE_INVALID) {

		ebike_conn_handle = p_evt->conn_handle;

		ebike_state_reset();

#if defined(CFG_SLAVE_LATENCY) && CFG_SLAVE_LATENCY > 0
		// ebike connection needs zero slave latency so incoming LDI notification performance remains high
		// our device is a peripheral in this connection and has to be awake to receive incoming data
		// new connections always get default values set in sd_ble_gap_ppcp_set() on init
		ble_gap_conn_params_t ebike_conn_params = {.min_conn_interval = MSEC_TO_UNITS(CFG_MIN_CONN_INTERVAL, UNIT_1_25_MS),
												   .max_conn_interval = MSEC_TO_UNITS(CFG_MAX_CONN_INTERVAL, UNIT_1_25_MS),
												   .slave_latency = 0,
												   .conn_sup_timeout = MSEC_TO_UNITS(CFG_CONN_SUP_TIMEOUT, UNIT_10_MS)};

		err_code = ble_conn_params_change_conn_params(p_evt->conn_handle, &ebike_conn_params);
		APP_ERROR_CHECK(err_code);
#endif

		err_code = ble_ldi_c_handles_assign(p_ble_ldi_c, p_evt->conn_handle, &p_evt->params.peer_db);
		APP_ERROR_CHECK(err_code);

		err_code = ble_ldi_c_ldi_read(p_ble_ldi_c);
		APP_ERROR_CHECK(err_code);

		err_code = ble_ldi_c_ldi_notif_enable(p_ble_ldi_c);
		APP_ERROR_CHECK(err_code);
	} else {
		NRF_LOG_DEBUG("Peer with LDI service already present on connection 0x%02X. Aborting", ebike_conn_handle);
		sd_ble_gap_disconnect(p_evt->conn_handle, BLE_HCI_MEMORY_CAPACITY_EXCEEDED);
	}
}

static void on_live_data_read(const ble_ldi_t *const p_incoming_msg)
{
	ASSERT(p_incoming_msg);

	ebike_state_cpy(&ebike_state, p_incoming_msg);

	ebike_state_data_t state_data = {
		.evt = EBIKE_STATE_EVT_INITIAL_DATA_READ,
		.params.initial_data_read.p_initial_data = p_incoming_msg,
	};

	notify_state_handlers(&state_data);
}

static void on_live_data_notif(const ble_ldi_t *const p_incoming_msg)
{
	ebike_state_data_t state_data = {.evt = EBIKE_STATE_EVT_LIVE_DATA_NOTIFICATION,
									 .params.live_data_notification = {
										 .p_previous_ebike_data = &ebike_state,
										 .p_incoming_notification = p_incoming_msg,
									 }};
	notify_state_handlers(&state_data);

	ebike_state_cpy(&ebike_state, p_incoming_msg);
}

static void on_ldi_c_disconnect(const ble_ldi_c_evt_t *const p_evt)
{
	ASSERT(p_evt);
	if(p_evt->conn_handle == ebike_conn_handle) {
		ebike_state_data_t state_data = {
			.evt = EBIKE_STATE_DISCONNECTED,
		};
		notify_state_handlers(&state_data);

		ebike_conn_handle = BLE_CONN_HANDLE_INVALID;
		ebike_state_reset();
	}
}

void ebike_state_on_ldi_c_evt(ble_ldi_c_t *p_ble_ldi_c, ble_ldi_c_evt_t *p_evt)
{
	ASSERT(p_ble_ldi_c);
	ASSERT(p_evt);

	switch(p_evt->evt_type) {
	case BLE_LDI_C_EVT_DISCOVERY_COMPLETE: {
		on_ldi_c_discovery_complete(p_ble_ldi_c, p_evt);
	} break;

	case BLE_LDI_C_EVT_LIVE_DATA_READ: {
		on_live_data_read(&p_evt->params.ldi);
	} break;

	case BLE_LDI_C_EVT_LIVE_DATA_NOTIFICATION: {
		on_live_data_notif(&p_evt->params.ldi);
	} break;

	case BLE_LDI_C_EVT_DISCONNECT: {
		on_ldi_c_disconnect(p_evt);
	} break;

	default:
		break;
	}
}

bool ebike_is_connected(void)
{
	return (ebike_conn_handle != BLE_CONN_HANDLE_INVALID);
}

ret_code_t ebike_state_register_handler(ebike_state_handler_t handler)
{
	ASSERT(handler);

	VERIFY_TRUE(state_handlers.len < EBIKE_STATE_MAX_HANDLERS, NRF_ERROR_NO_MEM);

	state_handlers.handlers[state_handlers.len++] = handler;

	return NRF_SUCCESS;
}

const ble_ldi_t *ebike_state_get(void)
{
	return &ebike_state;
}
