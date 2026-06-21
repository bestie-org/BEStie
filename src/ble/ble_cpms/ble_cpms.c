#include "ble_cpms.h"
#include "ble_conn_state.h"

#define NRF_LOG_MODULE_NAME CPMS
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// Cycling Power Measurement flags
#define BLE_CPMS_MEAS_FLAG_WHEEL_REVOLUTION_DATA_PRESENT (1 << 4)
#define BLE_CPMS_MEAS_FLAG_CRANK_REVOLUTION_DATA_PRESENT (1 << 5)

static ret_code_t is_cycling_power_measurement_notification_enabled(const ble_cpms_t *const p_cpms, const uint16_t conn_handle,
																	bool *p_notification_enabled)
{
	ASSERT(p_cpms);
	ASSERT(p_notification_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_cpms->cycling_power_measurement_handles.cccd_handle, &gatts_value);

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

static ret_code_t is_cycling_power_control_point_indication_enabled(const ble_cpms_t *const p_cpms, const uint16_t conn_handle,
																	bool *p_indication_enabled)
{
	ASSERT(p_cpms);
	ASSERT(p_indication_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_cpms->cycling_power_control_point_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_indication_enabled = ble_srv_is_indication_enabled(cccd_value_buf);
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_indication_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static size_t measurement_encode(const ble_cpms_data_t *const p_data, uint8_t *const p_buff, const size_t buff_len)
{
	ASSERT(p_data);
	ASSERT(p_buff);
	ASSERT(buff_len >= BLE_CPMS_MAX_SC_MEAS_LEN);

	memset(p_buff, 0, buff_len);

	uint16_t flags = 0;
	size_t len = 2;

	len += uint16_encode((uint16_t)p_data->instantaneous_power_W, &p_buff[len]);

	// Wheel Revolution Data field
	if(p_data->speed_present) {
		flags |= BLE_CPMS_MEAS_FLAG_WHEEL_REVOLUTION_DATA_PRESENT;

		// Cumulative Wheel Revolutions
		len += uint32_encode(p_data->total_wheel_revolutions, &p_buff[len]);

		// Last Wheel Event Time uint16
		len += uint16_encode(p_data->last_wheel_event, &p_buff[len]);
	}

	// Crank Revolution Data field
	if(p_data->cadence_present) {

		flags |= BLE_CPMS_MEAS_FLAG_CRANK_REVOLUTION_DATA_PRESENT;

		// Cumulative Crank Revolutions uint16_t
		len += uint16_encode(p_data->total_crank_revolutions, &p_buff[len]);

		// Last Crank Event Time uint16_t
		len += uint16_encode(p_data->last_crank_event, &p_buff[len]);
	}

	uint16_encode(flags, &p_buff[0]);
	ASSERT(len <= buff_len);

	return len;
}

ret_code_t ble_cpms_update(ble_cpms_t *const p_cpms, const ble_cpms_data_t *const p_data)
{
	ASSERT(p_cpms);
	ASSERT(p_data);

	ret_code_t err_code;

	if(p_data->speed_present && !(p_cpms->features & BLE_CPMS_FEATURE_WHEEL_REVOLUTION_DATA_BIT)) {
		return NRF_ERROR_INVALID_FLAGS;
	}

	if(p_data->cadence_present && !(p_cpms->features & BLE_CPMS_FEATURE_CRANK_REVOLUTION_DATA_BIT)) {
		return NRF_ERROR_INVALID_FLAGS;
	}

	uint8_t buff[BLE_CPMS_MAX_SC_MEAS_LEN] = {0};
	const size_t encoded_len = measurement_encode(p_data, buff, sizeof(buff));

	// this is overwritten by amount of bytes sent
	uint16_t len_buff[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];

	for(size_t i = 0; i < sizeof(len_buff) / sizeof(len_buff[0]); i++) {
		len_buff[i] = encoded_len;
	}

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_cpms->cycling_power_measurement_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &buff[0];
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_NOTIFICATION;

	ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_periph_handles();
	for(size_t i = 0; i < conn_handles.len; i++) {
		const uint16_t conn_handle = conn_handles.conn_handles[i];

		ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		if(p_cpms->cycling_power_measurement_notification_enabled[conn_handle] == false) {
			continue;
		}

		gq_req.params.gatts_hvx.p_len = &len_buff[conn_handle];
		err_code = nrf_ble_gq_item_add(p_cpms->p_gatt_queue, &gq_req, conn_handle);

		if(err_code != NRF_SUCCESS)
			return err_code;
	}

	return NRF_SUCCESS;
}

ret_code_t ble_cpms_init(ble_cpms_t *const p_cpms, const ble_cpms_init_t *const p_cpms_init)
{
	ASSERT(p_cpms);
	ASSERT(p_cpms_init);
	ASSERT(p_cpms_init->p_gatt_queue);

	ret_code_t err_code;
	ble_uuid_t ble_uuid;
	ble_add_char_params_t add_char_params;

	memset(p_cpms, 0, sizeof(ble_cpms_t));

	p_cpms->p_gatt_queue = p_cpms_init->p_gatt_queue;
	p_cpms->features = p_cpms_init->features;

	uint8_t cycling_power_feature_encoded[4] = {0};
	uint32_encode(p_cpms_init->features, cycling_power_feature_encoded);

	// Add service
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CYCLING_POWER_SERVICE);

	err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_cpms->service_handle);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Cycling Power Feature characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_CYCLING_POWER_FEATURE_CHAR;
	add_char_params.max_len = 4;
	add_char_params.init_len = 4;
	add_char_params.p_init_value = cycling_power_feature_encoded;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_cpms_init->cycling_power_feature_read_access;

	err_code = characteristic_add(p_cpms->service_handle, &add_char_params, &p_cpms->cycling_power_feature_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Cycling Power Measurement characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_CYCLING_POWER_MEASUREMENT_CHAR;
	add_char_params.max_len = BLE_CPMS_MAX_SC_MEAS_LEN;
	add_char_params.init_len = 0;
	add_char_params.p_init_value = NULL;
	add_char_params.is_var_len = true;
	add_char_params.char_props.notify = 1;
	add_char_params.cccd_write_access = p_cpms_init->cycling_power_measurement_cccd_write_access;

	err_code = characteristic_add(p_cpms->service_handle, &add_char_params, &p_cpms->cycling_power_measurement_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Sensor Location characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));
	uint8_t sensor_location_initial_val = p_cpms_init->sensor_location;

	add_char_params.uuid = BLE_UUID_SENSOR_LOCATION_CHAR;
	add_char_params.max_len = 1;
	add_char_params.init_len = 1;
	add_char_params.p_init_value = &sensor_location_initial_val;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_cpms_init->cycling_power_sensor_location_read_access;

	err_code = characteristic_add(p_cpms->service_handle, &add_char_params, &p_cpms->sensor_location_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add Control Point characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_CYCLING_POWER_CONTROL_POINT_CHAR;
	add_char_params.max_len = 19;
	add_char_params.init_len = 0;
	add_char_params.is_var_len = true;
	add_char_params.char_props.write = 1;
	add_char_params.char_props.indicate = 1;
	add_char_params.write_access = p_cpms_init->cycling_power_control_point_write_access;
	add_char_params.cccd_write_access = p_cpms_init->cycling_power_control_point_cccd_access;

	err_code = characteristic_add(p_cpms->service_handle, &add_char_params, &p_cpms->cycling_power_control_point_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	return err_code;
}

static void on_connect(ble_cpms_t *const p_cpms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_cpms);

	uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	ret_code_t err_code = nrf_ble_gq_conn_handle_register(p_cpms->p_gatt_queue, p_ble_evt->evt.gap_evt.conn_handle);
	APP_ERROR_CHECK(err_code);

	err_code = is_cycling_power_measurement_notification_enabled(
		p_cpms, conn_handle, &p_cpms->cycling_power_measurement_notification_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);

	err_code = is_cycling_power_control_point_indication_enabled(
		p_cpms, conn_handle, &p_cpms->cycling_power_control_point_indication_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);
}

static void on_disconnect(ble_cpms_t *const p_cpms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_cpms);

	const uint8_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);
	p_cpms->cycling_power_measurement_notification_enabled[conn_handle] = false;
	p_cpms->cycling_power_control_point_indication_enabled[conn_handle] = false;
}

static void on_cycling_power_ctrlpnt_write(ble_cpms_t *const p_cpms, ble_evt_t const *p_ble_evt)
{
	const uint8_t ctrlpnt_opcode = p_ble_evt->evt.gatts_evt.params.write.data[0];

	uint8_t indication_buff[] = {0x20, ctrlpnt_opcode, 0x04}; // send 'operation failed' to any request
	uint16_t indication_buff_len = sizeof(indication_buff);

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_cpms->cycling_power_control_point_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &indication_buff[0];
	gq_req.params.gatts_hvx.p_len = &indication_buff_len;
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_INDICATION;

	ret_code_t err_code = nrf_ble_gq_item_add(p_cpms->p_gatt_queue, &gq_req, p_ble_evt->evt.gatts_evt.conn_handle);
	APP_ERROR_CHECK(err_code);
}

static void on_write(ble_cpms_t *const p_cpms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_cpms);

	ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

	const uint8_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	if(p_evt_write->handle == p_cpms->cycling_power_measurement_handles.cccd_handle) {
		p_cpms->cycling_power_measurement_notification_enabled[conn_handle] =
			ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);

		if(p_cpms->cycling_power_measurement_notification_enabled[conn_handle]) {
			NRF_LOG_DEBUG("measurement_notification enabled on conn_handle 0x%02X", conn_handle);
		}
	} else if(p_evt_write->handle == p_cpms->cycling_power_control_point_handles.cccd_handle) {
		p_cpms->cycling_power_control_point_indication_enabled[conn_handle] =
			ble_srv_is_indication_enabled(p_ble_evt->evt.gatts_evt.params.write.data);
	} else if(p_evt_write->handle == p_cpms->cycling_power_control_point_handles.value_handle &&
			  p_cpms->cycling_power_control_point_indication_enabled[conn_handle]) {
		on_cycling_power_ctrlpnt_write(p_cpms, p_ble_evt);
	}
}

void ble_cpms_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ASSERT(p_ble_evt);
	ASSERT(p_context);

	ble_cpms_t *const p_cpms = (ble_cpms_t *)p_context;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(p_cpms, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(p_cpms, p_ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(p_cpms, p_ble_evt);
		break;

	default:
		// No implementation needed.
		break;
	}
}
