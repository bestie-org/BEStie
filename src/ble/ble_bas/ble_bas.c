#include "ble_bas.h"
#include "ble_conn_state.h"

#define NRF_LOG_MODULE_NAME BAS
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static ret_code_t is_battery_level_notification_enabled(const ble_bas_t *const p_bas, const uint16_t conn_handle,
														bool *p_notification_enabled)
{
	ASSERT(p_bas);
	ASSERT(p_notification_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_bas->battery_level_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_notification_enabled = ble_srv_is_notification_enabled(cccd_value_buf);

		if(*p_notification_enabled) {
			NRF_LOG_DEBUG("battery level notification enabled on conn_handle 0x%02X", conn_handle);
		}
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_notification_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

ret_code_t ble_bas_update(ble_bas_t *const p_bas, const uint8_t battery_level_pct)
{
	ASSERT(p_bas);

	ret_code_t err_code;
	bool no_mem_occurred = false;

	ble_gatts_value_t gatts_value = {0};

	gatts_value.len = sizeof(uint8_t);
	gatts_value.offset = 0;
	gatts_value.p_value = (uint8_t *)&battery_level_pct;

	// Update characteristic value in softdevice.
	err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, p_bas->battery_level_handles.value_handle, &gatts_value);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// this is overwritten by amount of bytes sent
	uint16_t len_buff[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];

	for(size_t i = 0; i < sizeof(len_buff) / sizeof(len_buff[0]); i++) {
		len_buff[i] = sizeof(uint8_t);
	}

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_bas->battery_level_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &battery_level_pct;
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_NOTIFICATION;

	ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_periph_handles();
	for(size_t i = 0; i < conn_handles.len; i++) {
		const uint16_t conn_handle = conn_handles.conn_handles[i];

		ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		if(p_bas->battery_level_notification_enabled[conn_handle] == false) {
			continue;
		}

		gq_req.params.gatts_hvx.p_len = &len_buff[conn_handle];
		err_code = nrf_ble_gq_item_add(p_bas->p_gatt_queue, &gq_req, conn_handle);

		// NRF_ERROR_NO_MEM may be global (data pool) or per connection
		// give other peers a chance to get the notification
		if(err_code == NRF_ERROR_NO_MEM) {
			no_mem_occurred = true;
			continue;
		}

		if(err_code != NRF_SUCCESS) {
			return err_code;
		}
	}

	if(no_mem_occurred) {
		return NRF_ERROR_NO_MEM;
	}
	return NRF_SUCCESS;
}

ret_code_t ble_bas_init(ble_bas_t *const p_bas, const ble_bas_init_t *const p_bas_init)
{
	ASSERT(p_bas);
	ASSERT(p_bas_init);
	ASSERT(p_bas_init->p_gatt_queue);

	ret_code_t err_code;
	ble_uuid_t ble_uuid;
	ble_add_char_params_t add_char_params;

	memset(p_bas, 0, sizeof(ble_bas_t));

	p_bas->p_gatt_queue = p_bas_init->p_gatt_queue;

	uint8_t init_battery_level = 100;

	// Add service
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BATTERY_SERVICE);

	err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_bas->service_handle);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Battery Level characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_BATTERY_LEVEL_CHAR;
	add_char_params.max_len = sizeof(uint8_t);
	add_char_params.init_len = sizeof(uint8_t);
	add_char_params.is_var_len = false;
	add_char_params.p_init_value = &init_battery_level;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_bas_init->battery_level_read_access;
	add_char_params.char_props.notify = 1;
	add_char_params.cccd_write_access = p_bas_init->battery_level_cccd_access;

	err_code = characteristic_add(p_bas->service_handle, &add_char_params, &p_bas->battery_level_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	return err_code;
}

static void on_connect(ble_bas_t *const p_bas, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_bas);

	uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	ret_code_t err_code = nrf_ble_gq_conn_handle_register(p_bas->p_gatt_queue, p_ble_evt->evt.gap_evt.conn_handle);
	APP_ERROR_CHECK(err_code);

	err_code = is_battery_level_notification_enabled(p_bas, conn_handle, &p_bas->battery_level_notification_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);
}

static void on_disconnect(ble_bas_t *const p_bas, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_bas);

	const uint8_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);
	p_bas->battery_level_notification_enabled[conn_handle] = false;
}

static void on_write(ble_bas_t *const p_bas, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_bas);

	ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

	const uint8_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	if(p_evt_write->handle == p_bas->battery_level_handles.cccd_handle) {
		p_bas->battery_level_notification_enabled[conn_handle] =
			ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);

		if(p_bas->battery_level_notification_enabled[conn_handle]) {
			NRF_LOG_DEBUG("battery_level_notification enabled on conn_handle 0x%02X", conn_handle);
		}
	}
}

void ble_bas_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ASSERT(p_ble_evt);
	ASSERT(p_context);

	ble_bas_t *const p_bas = (ble_bas_t *)p_context;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(p_bas, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(p_bas, p_ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(p_bas, p_ble_evt);
		break;

	default:
		// No implementation needed.
		break;
	}
}
