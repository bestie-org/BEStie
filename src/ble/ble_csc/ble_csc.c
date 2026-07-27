#include "ble_csc.h"
#include "ble_conn_state.h"

#define NRF_LOG_MODULE_NAME CSC
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// Cycling Speed and Cadence flags
#define BLE_CSC_MEAS_FLAG_WHEEL_REVOLUTION_DATA_PRESENT (1 << 0)
#define BLE_CSC_MEAS_FLAG_CRANK_REVOLUTION_DATA_PRESENT (1 << 1)

static ret_code_t is_sc_measurement_notification_enabled(const ble_csc_t *const p_csc, const uint16_t conn_handle,
														 bool *p_notification_enabled)
{
	ASSERT(p_csc);
	ASSERT(p_notification_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_csc->sc_measurement_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_notification_enabled = ble_srv_is_notification_enabled(cccd_value_buf);

		if(*p_notification_enabled) {
			NRF_LOG_DEBUG("measurement_notification enabled on conn_handle 0x%02X", conn_handle);
		}
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_notification_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static ret_code_t is_sc_control_point_indication_enabled(const ble_csc_t *const p_csc, const uint16_t conn_handle,
														 bool *p_indication_enabled)
{
	ASSERT(p_csc);
	ASSERT(p_indication_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_csc->sc_control_point_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_indication_enabled = ble_srv_is_indication_enabled(cccd_value_buf);
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_indication_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static size_t sc_measurement_encode(const ble_csc_data_t *const p_data, uint8_t *const p_buff, const size_t buff_len)
{
	ASSERT(p_data);
	ASSERT(p_buff);
	ASSERT(buff_len >= BLE_CSC_MAX_SC_MEAS_LEN);

	memset(p_buff, 0, buff_len);

	uint8_t flags = 0;
	size_t len = 1;

	// Wheel Revolution Data field
	if(p_data->speed_present) {
		flags |= BLE_CSC_MEAS_FLAG_WHEEL_REVOLUTION_DATA_PRESENT;

		// Cumulative Wheel Revolutions
		len += uint32_encode(p_data->total_wheel_revolutions, &p_buff[len]);

		// Last Wheel Event Time uint16
		len += uint16_encode(p_data->last_wheel_event, &p_buff[len]);
	}

	// Crank Revolution Data field
	if(p_data->cadence_present) {

		flags |= BLE_CSC_MEAS_FLAG_CRANK_REVOLUTION_DATA_PRESENT;

		// Cumulative Crank Revolutions uint16_t
		len += uint16_encode(p_data->total_crank_revolutions, &p_buff[len]);

		// Last Crank Event Time uint16_t
		len += uint16_encode(p_data->last_crank_event, &p_buff[len]);
	}

	p_buff[0] = flags;

	ASSERT(len <= buff_len);

	return len;
}

ret_code_t ble_csc_measurement_update(ble_csc_t *const p_csc, const ble_csc_data_t *const p_data)
{
	ASSERT(p_csc);
	ASSERT(p_data);

	ret_code_t err_code;
	bool no_mem_occurred = false;

	if(!p_data->speed_present && !p_data->cadence_present) {
		// no update needed
		return NRF_SUCCESS;
	}

	if(p_data->speed_present && !(p_csc->features & BLE_CSC_FEATURE_WHEEL_REVOLUTION_DATA_BIT)) {
		return NRF_ERROR_INVALID_FLAGS;
	}

	if(p_data->cadence_present && !(p_csc->features & BLE_CSC_FEATURE_CRANK_REVOLUTION_DATA_BIT)) {
		return NRF_ERROR_INVALID_FLAGS;
	}

	uint8_t buff[BLE_CSC_MAX_SC_MEAS_LEN] = {0};
	const size_t encoded_len = sc_measurement_encode(p_data, buff, sizeof(buff));

	// this is overwritten by amount of bytes sent
	uint16_t len_buff[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];

	for(size_t i = 0; i < sizeof(len_buff) / sizeof(len_buff[0]); i++) {
		len_buff[i] = encoded_len;
	}

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_csc->sc_measurement_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &buff[0];
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_NOTIFICATION;

	ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_periph_handles();
	for(size_t i = 0; i < conn_handles.len; i++) {
		const uint16_t conn_handle = conn_handles.conn_handles[i];

		ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		if(p_csc->sc_measurement_notification_enabled[conn_handle] == false) {
			continue;
		}

		gq_req.params.gatts_hvx.p_len = &len_buff[conn_handle];
		err_code = nrf_ble_gq_item_add(p_csc->p_gatt_queue, &gq_req, conn_handle);

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

ret_code_t ble_csc_init(ble_csc_t *const p_csc, const ble_csc_init_t *const p_csc_init)
{
	ASSERT(p_csc);
	ASSERT(p_csc_init);
	ASSERT(p_csc_init->p_gatt_queue);

	ret_code_t err_code;
	ble_uuid_t ble_uuid;
	ble_add_char_params_t add_char_params;

	memset(p_csc, 0, sizeof(ble_csc_t));

	p_csc->p_gatt_queue = p_csc_init->p_gatt_queue;
	p_csc->features = p_csc_init->features;

	uint8_t sc_features_encoded[2] = {0};
	uint16_encode(p_csc_init->features, sc_features_encoded);

	// Add service
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CYCLING_SPEED_AND_CADENCE);

	err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_csc->service_handle);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add CSC Measurement characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_CSC_MEASUREMENT_CHAR;
	add_char_params.max_len = BLE_CSC_MAX_SC_MEAS_LEN;
	add_char_params.init_len = 0;
	add_char_params.p_init_value = NULL;
	add_char_params.is_var_len = true;
	add_char_params.char_props.notify = 1;
	add_char_params.cccd_write_access = p_csc_init->sc_measurement_cccd_access;

	err_code = characteristic_add(p_csc->service_handle, &add_char_params, &p_csc->sc_measurement_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add CSC Feature characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_CSC_FEATURE_CHAR;
	add_char_params.max_len = 2;
	add_char_params.init_len = 2;
	add_char_params.p_init_value = sc_features_encoded;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_csc_init->sc_feature_read_access;

	err_code = characteristic_add(p_csc->service_handle, &add_char_params, &p_csc->sc_feature_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Sensor Location characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));
	uint8_t sensor_location_initial_val = p_csc_init->sensor_location;

	add_char_params.uuid = BLE_UUID_SENSOR_LOCATION_CHAR;
	add_char_params.max_len = 1;
	add_char_params.init_len = 1;
	add_char_params.p_init_value = &sensor_location_initial_val;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_csc_init->sensor_location_read_access;

	err_code = characteristic_add(p_csc->service_handle, &add_char_params, &p_csc->sensor_location_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add SC Control Point characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_SC_CTRLPT_CHAR;
	add_char_params.max_len = 6;
	add_char_params.init_len = 0;
	add_char_params.is_var_len = true;
	add_char_params.char_props.write = 1;
	add_char_params.char_props.indicate = 1;
	add_char_params.write_access = p_csc_init->sc_control_point_write_access;
	add_char_params.cccd_write_access = p_csc_init->sc_control_point_cccd_access;

	err_code = characteristic_add(p_csc->service_handle, &add_char_params, &p_csc->sc_control_point_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	return err_code;
}

static void on_connect(ble_csc_t *const p_csc, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_csc);

	uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	ret_code_t err_code = nrf_ble_gq_conn_handle_register(p_csc->p_gatt_queue, p_ble_evt->evt.gap_evt.conn_handle);
	APP_ERROR_CHECK(err_code);

	err_code =
		is_sc_measurement_notification_enabled(p_csc, conn_handle, &p_csc->sc_measurement_notification_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);

	err_code =
		is_sc_control_point_indication_enabled(p_csc, conn_handle, &p_csc->sc_control_point_indication_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);
}

static void on_disconnect(ble_csc_t *const p_csc, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_csc);

	const uint8_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);
	p_csc->sc_measurement_notification_enabled[conn_handle] = false;
	p_csc->sc_control_point_indication_enabled[conn_handle] = false;
}

static void on_sc_ctrlpnt_write(ble_csc_t *const p_csc, ble_evt_t const *p_ble_evt)
{
	const uint8_t ctrlpnt_opcode = p_ble_evt->evt.gatts_evt.params.write.data[0];

	uint8_t indication_buff[] = {0x10, ctrlpnt_opcode, 0x04}; // send 'operation failed' to any request
	uint16_t indication_buff_len = sizeof(indication_buff);

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_csc->sc_control_point_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &indication_buff[0];
	gq_req.params.gatts_hvx.p_len = &indication_buff_len;
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_INDICATION;

	ret_code_t err_code = nrf_ble_gq_item_add(p_csc->p_gatt_queue, &gq_req, p_ble_evt->evt.gatts_evt.conn_handle);
	APP_ERROR_CHECK(err_code);
}

static void on_write(ble_csc_t *const p_csc, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_csc);

	ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

	const uint8_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	if(p_evt_write->handle == p_csc->sc_measurement_handles.cccd_handle) {
		p_csc->sc_measurement_notification_enabled[conn_handle] =
			ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);

		if(p_csc->sc_measurement_notification_enabled[conn_handle]) {
			NRF_LOG_DEBUG("measurement_notification enabled on conn_handle 0x%02X", conn_handle);
		}

	} else if(p_evt_write->handle == p_csc->sc_control_point_handles.cccd_handle) {
		p_csc->sc_control_point_indication_enabled[conn_handle] =
			ble_srv_is_indication_enabled(p_ble_evt->evt.gatts_evt.params.write.data);
	} else if(p_evt_write->handle == p_csc->sc_control_point_handles.value_handle &&
			  p_csc->sc_control_point_indication_enabled[conn_handle]) {
		on_sc_ctrlpnt_write(p_csc, p_ble_evt);
	}
}

void ble_csc_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ASSERT(p_ble_evt);
	ASSERT(p_context);

	ble_csc_t *const p_csc = (ble_csc_t *)p_context;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(p_csc, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(p_csc, p_ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(p_csc, p_ble_evt);
		break;

	default:
		// No implementation needed.
		break;
	}
}
