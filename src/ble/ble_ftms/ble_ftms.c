#include "ble_ftms.h"
#include "ble_conn_state.h"

#define NRF_LOG_MODULE_NAME FTMS
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// FTMS Fitness Machine Feature flags
#define BLE_FTMS_FEATURE_CADENCE_SUPPORTED			 (1 << 1)
#define BLE_FTMS_FEATURE_TOTAL_DISTANCE_SUPPORTED	 (1 << 2)
#define BLE_FTMS_FEATURE_POWER_MEASUREMENT_SUPPORTED (1 << 14)

// FTMS Target Setting flags
#define BLE_FTMS_TGT_FEATURE_WHEEL_CIRCUMFERENCE (1 << 14) // Wheel Circumference Configuration Supported

// Indoor Bike Data char flags
#define BLE_FTMS_FLAG_MORE_DATA						(1 << 0)
#define BLE_FTMS_FLAG_AVG_SPEED_PRESENT				(1 << 1)
#define BLE_FTMS_FLAG_INSTANTANEOUS_CADENCE_PRESENT (1 << 2)
#define BLE_FTMS_FLAG_AVERAGE_CADENCE_PRESENT		(1 << 3)
#define BLE_FTMS_FLAG_TOTAL_DISTANCE_PRESENT		(1 << 4)
#define BLE_FTMS_FLAG_RESISTANCE_LEVEL_PRESENT		(1 << 5)
#define BLE_FTMS_FLAG_INSTANTANEOUS_POWER_PRESENT	(1 << 6)
#define BLE_FTMS_FLAG_AVERAGE_POWER_PRESENT			(1 << 7)
#define BLE_FTMS_FLAG_EXPENDED_ENERGY_PRESENT		(1 << 8)
#define BLE_FTMS_FLAG_HEART_RATE_PRESENT			(1 << 9)
#define BLE_FTMS_FLAG_METABOLIC_EQUIVALENT_PRESENT	(1 << 10)
#define BLE_FTMS_FLAG_ELAPSED_TIME_PRESENT			(1 << 11)
#define BLE_FTMS_FLAG_REMAINING_TIME_PRESENT		(1 << 12)

typedef enum
{
	BLE_FTMS_CTRLPNT_OP_REQUEST_CONTROL = 0,
	BLE_FTMS_CTRLPNT_OP_RESET,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_SPEED,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_INCLINATION,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_RESISTANCE_LEVEL,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_POWER,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_HR,
	BLE_FTMS_CTRLPNT_OP_START_OR_RESUME,
	BLE_FTMS_CTRLPNT_OP_STOP_OR_PAUSE,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_EXPENDED_ENERGY,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_NUMBER_OF_STEPS,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_NUMBER_OF_STRIDES,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_DISTANCE,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_TRAINING_TIME,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_TIME_IN_2_HR_ZONES,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_TIME_IN_3_HR_ZONES,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_TIME_IN_5_HR_ZONES,
	BLE_FTMS_CTRLPNT_OP_SET_INDOOR_BIKE_SIMULATION_PARAMS,
	BLE_FTMS_CTRLPNT_OP_SET_WHEEL_CIRCUMFERENCE,
	BLE_FTMS_CTRLPNT_OP_SPIN_DOWN_CTRL,
	BLE_FTMS_CTRLPNT_OP_SET_TGT_CADENCE,
	BLE_FTMS_CTRLPNT_OP_RESPONSE = 0x80,
} ble_ftms_ctrlpnt_opcode_t;

typedef enum
{
	BLE_FTMS_CTRLPNT_RSP_SUCCESS = 0x01,
	BLE_FTMS_CTRLPNT_RSP_OP_NOT_SUPPORTED,
	BLE_FTMS_CTRLPNT_RSP_INVALID_PARAM,
	BLE_FTMS_CTRLPNT_RSP_OP_FAILED,
	BLE_FTMS_CTRLPNT_RSP_CONTROL_NOT_PERMITTED,
} ble_ftms_ctrlpnt_response_code_t;

// Fitness Machine Status Op Codes
typedef enum
{
	BLE_FTMS_STATUS_OP_RESET = 0x01,
	BLE_FTMS_STATUS_OP_STOPPED_OR_PAUSED = 0x02,
	BLE_FTMS_STATUS_OP_STARTED_OR_RESUMED = 0x04,
	BLE_FTMS_STATUS_OP_CONTROL_PERMISSION_LOST = 0xFF,
} ble_ftms_status_opcode_t;

typedef enum
{
	BLE_FTMS_OP_STOPPED_OR_PAUSED_STOP = 0x01,
	BLE_FTMS_OP_STOPPED_OR_PAUSED_PAUSE = 0x02,
} ble_ftms_op_stopped_or_paused_t;

static ret_code_t is_indoor_bike_data_notification_enabled(const ble_ftms_t *const p_ftms, const uint16_t conn_handle,
														   bool *p_notification_enabled)
{
	ASSERT(p_ftms);
	ASSERT(p_notification_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_ftms->indoor_bike_data_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_notification_enabled = ble_srv_is_notification_enabled(cccd_value_buf);

		if(*p_notification_enabled) {
			NRF_LOG_DEBUG("indoor bike notif enabled on conn_handle 0x%02X", conn_handle);
		}
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_notification_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static ret_code_t is_ftms_status_notification_enabled(const ble_ftms_t *const p_ftms, const uint16_t conn_handle,
													  bool *p_notification_enabled)
{
	ASSERT(p_ftms);
	ASSERT(p_notification_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_ftms->ftms_status_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_notification_enabled = ble_srv_is_notification_enabled(cccd_value_buf);
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_notification_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static ret_code_t is_ftms_control_point_indication_enabled(const ble_ftms_t *const p_ftms, const uint16_t conn_handle,
														   bool *p_indication_enabled)
{
	ASSERT(p_ftms);
	ASSERT(p_indication_enabled);

	ret_code_t err_code;
	uint8_t cccd_value_buf[BLE_CCCD_VALUE_LEN];
	ble_gatts_value_t gatts_value;

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len = BLE_CCCD_VALUE_LEN;
	gatts_value.offset = 0;
	gatts_value.p_value = cccd_value_buf;

	err_code = sd_ble_gatts_value_get(conn_handle, p_ftms->ftms_control_point_handles.cccd_handle, &gatts_value);

	if(err_code == NRF_SUCCESS) {
		*p_indication_enabled = ble_srv_is_indication_enabled(cccd_value_buf);
	}
	if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
		*p_indication_enabled = false;
		return NRF_SUCCESS;
	}
	return err_code;
}

static size_t indoor_bike_measurement_encode(const ble_ftms_data_t *const p_data, uint8_t *const p_buff, const size_t buff_len)
{
	ASSERT(p_data);
	ASSERT(p_buff);
	ASSERT(buff_len >= BLE_FTMS_MAX_INDOOR_BIKE_DATA_LEN);

	memset(p_buff, 0, buff_len);

	uint16_t flags = BLE_FTMS_FLAG_MORE_DATA;
	size_t len = sizeof(flags);

	if(p_data->instantaneous_speed_present) {
		// special case: 'more data' flag shall be 0
		flags = 0;

		// Unit is 1/100 of a kilometer per hour
		len += uint16_encode((uint16_t)(p_data->instantaneous_speed_kph * 100.0f), &p_buff[len]);
	}

	if(p_data->instantaneous_cadence_present) {

		flags |= BLE_FTMS_FLAG_INSTANTANEOUS_CADENCE_PRESENT;

		// Unit is 1/2 of a revolution per minute
		len += uint16_encode(p_data->instantaneous_cadence_rpm * 2, &p_buff[len]);
	}

	if(p_data->average_cadence_present) {

		flags |= BLE_FTMS_FLAG_AVERAGE_CADENCE_PRESENT;

		// Unit is 1/2 of a revolution per minute
		len += uint16_encode(p_data->average_cadence_rpm * 2, &p_buff[len]);
	}

	if(p_data->total_distance_present) {

		flags |= BLE_FTMS_FLAG_TOTAL_DISTANCE_PRESENT;
		len += uint24_encode(p_data->total_distance_m, &p_buff[len]);
	}

	if(p_data->instantaneous_power_present) {

		flags |= BLE_FTMS_FLAG_INSTANTANEOUS_POWER_PRESENT;
		len += uint16_encode((uint16_t)p_data->instantaneous_power_W, &p_buff[len]);
	}

	if(p_data->average_power_present) {

		flags |= BLE_FTMS_FLAG_AVERAGE_POWER_PRESENT;
		len += uint16_encode((uint16_t)p_data->average_power_W, &p_buff[len]);
	}

	uint16_encode(flags, &p_buff[0]);
	ASSERT(len <= buff_len);

	return len;
}

ret_code_t ble_ftms_measurement_update(ble_ftms_t *const p_ftms, const ble_ftms_data_t *const p_data)
{
	ASSERT(p_ftms);
	ASSERT(p_data);

	ret_code_t err_code;
	bool no_mem_occurred = false;

	if(!p_data->instantaneous_speed_present && !p_data->instantaneous_cadence_present && !p_data->average_cadence_present &&
	   !p_data->total_distance_present && !p_data->instantaneous_power_present && !p_data->average_power_present) {
		// no update needed
		return NRF_SUCCESS;
	}

	uint8_t buff[BLE_FTMS_MAX_INDOOR_BIKE_DATA_LEN] = {0};
	const size_t encoded_len = indoor_bike_measurement_encode(p_data, buff, sizeof(buff));

	// this is overwritten by amount of bytes sent
	uint16_t len_buff[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];

	for(size_t i = 0; i < sizeof(len_buff) / sizeof(len_buff[0]); i++) {
		len_buff[i] = encoded_len;
	}

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_ftms->indoor_bike_data_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &buff[0];
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_NOTIFICATION;

	ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_periph_handles();
	for(size_t i = 0; i < conn_handles.len; i++) {
		const uint16_t conn_handle = conn_handles.conn_handles[i];

		ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		if(p_ftms->indoor_bike_data_notification_enabled[conn_handle] == false) {
			continue;
		}

		gq_req.params.gatts_hvx.p_len = &len_buff[conn_handle];
		err_code = nrf_ble_gq_item_add(p_ftms->p_gatt_queue, &gq_req, conn_handle);

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

ret_code_t ble_ftms_status_send(ble_ftms_t *const p_ftms, ble_ftms_status_t status)
{
	ASSERT(p_ftms);

	ret_code_t err_code;
	bool no_mem_occurred = false;

	uint8_t buff[2] = {0};
	uint8_t packet_len;

	switch(status) {
	case BLE_FTMS_STATUS_PAUSED:
		buff[0] = BLE_FTMS_STATUS_OP_STOPPED_OR_PAUSED;
		buff[1] = BLE_FTMS_OP_STOPPED_OR_PAUSED_PAUSE;
		packet_len = 2;
		break;
	case BLE_FTMS_STATUS_RESUMED:
		buff[0] = BLE_FTMS_STATUS_OP_STARTED_OR_RESUMED;
		packet_len = 1;
		break;
	default:
		return NRF_ERROR_INVALID_PARAM;
	}

	uint16_t len_buff[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	for(size_t i = 0; i < sizeof(len_buff) / sizeof(len_buff[0]); i++) {
		len_buff[i] = packet_len;
	}

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_ftms->ftms_status_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &buff[0];
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_NOTIFICATION;

	ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_periph_handles();
	for(size_t i = 0; i < conn_handles.len; i++) {
		const uint16_t conn_handle = conn_handles.conn_handles[i];

		ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		if(p_ftms->ftms_status_notification_enabled[conn_handle] == false) {
			continue;
		}

		gq_req.params.gatts_hvx.p_len = &len_buff[conn_handle];
		err_code = nrf_ble_gq_item_add(p_ftms->p_gatt_queue, &gq_req, conn_handle);

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

ret_code_t ble_ftms_init(ble_ftms_t *const p_ftms, const ble_ftms_init_t *const p_ftms_init)
{
	ASSERT(p_ftms);
	ASSERT(p_ftms_init);
	ASSERT(p_ftms_init->p_gatt_queue);

	ret_code_t err_code;
	ble_uuid_t ble_uuid;
	ble_add_char_params_t add_char_params;

	const uint32_t fitness_machine_features = BLE_FTMS_FEATURE_CADENCE_SUPPORTED | BLE_FTMS_FEATURE_TOTAL_DISTANCE_SUPPORTED |
											  BLE_FTMS_FEATURE_POWER_MEASUREMENT_SUPPORTED;

	const uint32_t target_setting_features = BLE_FTMS_TGT_FEATURE_WHEEL_CIRCUMFERENCE;

	memset(p_ftms, 0, sizeof(ble_ftms_t));

	p_ftms->p_gatt_queue = p_ftms_init->p_gatt_queue;

	uint8_t ftms_features_encoded[8] = {0};
	uint32_encode(fitness_machine_features, &ftms_features_encoded[0]);
	uint32_encode(target_setting_features, &ftms_features_encoded[4]);

	// Add service
	BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_FTMS_SERVICE);

	err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_ftms->service_handle);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add FTMS Feature characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_FTMS_FEATURE_CHAR;
	add_char_params.max_len = 8;
	add_char_params.init_len = 8;
	add_char_params.p_init_value = ftms_features_encoded;
	add_char_params.char_props.read = 1;
	add_char_params.read_access = p_ftms_init->ftms_feature_read_access;

	err_code = characteristic_add(p_ftms->service_handle, &add_char_params, &p_ftms->ftms_feature_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add FTMS Measurement characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_FTMS_INDOOR_BIKE_DATA_CHAR;
	add_char_params.max_len = BLE_FTMS_MAX_INDOOR_BIKE_DATA_LEN;
	add_char_params.init_len = 0;
	add_char_params.p_init_value = NULL;
	add_char_params.is_var_len = true;
	add_char_params.char_props.notify = 1;
	add_char_params.cccd_write_access = p_ftms_init->indoor_bike_data_cccd_access;

	err_code = characteristic_add(p_ftms->service_handle, &add_char_params, &p_ftms->indoor_bike_data_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add FTMS Control Point characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_FTMS_CONTROL_POINT_CHAR;
	add_char_params.max_len = 10;
	add_char_params.init_len = 0;
	add_char_params.is_var_len = true;
	add_char_params.char_props.write = 1;
	add_char_params.char_props.indicate = 1;
	add_char_params.write_access = p_ftms_init->ftms_control_point_write_access;
	add_char_params.cccd_write_access = p_ftms_init->ftms_control_point_cccd_access;

	err_code = characteristic_add(p_ftms->service_handle, &add_char_params, &p_ftms->ftms_control_point_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	// Add FTMS Status characteristic
	memset(&add_char_params, 0, sizeof(add_char_params));

	add_char_params.uuid = BLE_UUID_FTMS_STATUS_CHAR;
	add_char_params.max_len = 10;
	add_char_params.init_len = 0;
	add_char_params.p_init_value = NULL;
	add_char_params.is_var_len = true;
	add_char_params.char_props.notify = 1;
	add_char_params.cccd_write_access = p_ftms_init->ftms_status_cccd_access;

	err_code = characteristic_add(p_ftms->service_handle, &add_char_params, &p_ftms->ftms_status_handles);
	if(err_code != NRF_SUCCESS) {
		return err_code;
	}

	return err_code;
}

static void on_connect(ble_ftms_t *const p_ftms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_ftms);

	uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	ret_code_t err_code = nrf_ble_gq_conn_handle_register(p_ftms->p_gatt_queue, p_ble_evt->evt.gap_evt.conn_handle);
	APP_ERROR_CHECK(err_code);

	err_code = is_indoor_bike_data_notification_enabled(p_ftms, conn_handle,
														&p_ftms->indoor_bike_data_notification_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);

	err_code = is_ftms_status_notification_enabled(p_ftms, conn_handle, &p_ftms->ftms_status_notification_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);

	err_code =
		is_ftms_control_point_indication_enabled(p_ftms, conn_handle, &p_ftms->ftms_control_indication_enabled[conn_handle]);
	APP_ERROR_CHECK(err_code);
}

static void on_disconnect(ble_ftms_t *const p_ftms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_ftms);

	const uint8_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	p_ftms->indoor_bike_data_notification_enabled[conn_handle] = false;
	p_ftms->ftms_status_notification_enabled[conn_handle] = false;
	p_ftms->ftms_control_indication_enabled[conn_handle] = false;
}

static void on_sc_ctrlpnt_write(ble_ftms_t *const p_ftms, ble_evt_t const *p_ble_evt)
{
	const uint8_t ctrlpnt_opcode = p_ble_evt->evt.gatts_evt.params.write.data[0];

	uint8_t indication_buff[] = {BLE_FTMS_CTRLPNT_OP_RESPONSE, ctrlpnt_opcode,
								 BLE_FTMS_CTRLPNT_RSP_CONTROL_NOT_PERMITTED}; // send 'control not permitted' to any request
	uint16_t indication_buff_len = sizeof(indication_buff);

	nrf_ble_gq_req_t gq_req = {0};
	gq_req.type = NRF_BLE_GQ_REQ_GATTS_HVX;
	gq_req.params.gatts_hvx.handle = p_ftms->ftms_control_point_handles.value_handle;
	gq_req.params.gatts_hvx.offset = 0;
	gq_req.params.gatts_hvx.p_data = &indication_buff[0];
	gq_req.params.gatts_hvx.p_len = &indication_buff_len;
	gq_req.params.gatts_hvx.type = BLE_GATT_HVX_INDICATION;

	ret_code_t err_code = nrf_ble_gq_item_add(p_ftms->p_gatt_queue, &gq_req, p_ble_evt->evt.gatts_evt.conn_handle);
	APP_ERROR_CHECK(err_code);
}

static void on_write(ble_ftms_t *const p_ftms, ble_evt_t const *p_ble_evt)
{
	ASSERT(p_ble_evt);
	ASSERT(p_ftms);

	ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

	const uint8_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
	ASSERT(conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	if(p_evt_write->handle == p_ftms->indoor_bike_data_handles.cccd_handle) {
		p_ftms->indoor_bike_data_notification_enabled[conn_handle] =
			ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);

		if(p_ftms->indoor_bike_data_notification_enabled[conn_handle]) {
			NRF_LOG_DEBUG("notification enabled on conn_handle 0x%02X", conn_handle);
		}

	} else if(p_evt_write->handle == p_ftms->ftms_status_handles.cccd_handle) {
		p_ftms->ftms_status_notification_enabled[conn_handle] =
			ble_srv_is_notification_enabled(p_ble_evt->evt.gatts_evt.params.write.data);

	} else if(p_evt_write->handle == p_ftms->ftms_control_point_handles.cccd_handle) {
		p_ftms->ftms_control_indication_enabled[conn_handle] =
			ble_srv_is_indication_enabled(p_ble_evt->evt.gatts_evt.params.write.data);
	} else if(p_evt_write->handle == p_ftms->ftms_control_point_handles.value_handle &&
			  p_ftms->ftms_control_indication_enabled[conn_handle]) {
		on_sc_ctrlpnt_write(p_ftms, p_ble_evt);
	}
}

void ble_ftms_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ASSERT(p_ble_evt);
	ASSERT(p_context);

	ble_ftms_t *const p_ftms = (ble_ftms_t *)p_context;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(p_ftms, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(p_ftms, p_ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(p_ftms, p_ble_evt);
		break;

	default:
		// No implementation needed.
		break;
	}
}
