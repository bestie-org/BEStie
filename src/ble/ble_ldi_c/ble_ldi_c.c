#include "ble_ldi_c.h"
#include "ble_db_discovery.h"
#include "ble_gattc.h"
#include "ble_types.h"
#include "pb_decode.h"

#define NRF_LOG_LEVEL		NRF_LOG_SEVERITY_WARNING
#define NRF_LOG_MODULE_NAME ble_ldi_c
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define BLE_UUID_LDI_BASE {0xE4, 0xCC, 0xDB, 0xE2, 0x2A, 0x2A, 0xB4, 0x81, 0xE9, 0x11, 0xA2, 0xEA, 0x00, 0x00, 0x00, 0x00}

static const ble_uuid128_t ldi_base_uuid = {.uuid128 = BLE_UUID_LDI_BASE};

static uint8_t ldi_base_uuid_type = BLE_UUID_TYPE_UNKNOWN;

static void init_base_uuid(void)
{
	ret_code_t err_code = sd_ble_uuid_vs_add(&ldi_base_uuid, &ldi_base_uuid_type);
	APP_ERROR_CHECK(err_code);
}

uint8_t ble_ldi_c_get_base_type(void)
{
	if(ldi_base_uuid_type == BLE_UUID_TYPE_UNKNOWN) {
		init_base_uuid();
	}

	return ldi_base_uuid_type;
}

/**@brief Function for interception of the errors of GATTC and the BLE GATT Queue.
 *
 * @param[in] nrf_error   Error code.
 * @param[in] p_ctx       Parameter from the event handler.
 * @param[in] conn_handle Connection handle.
 */
static void gatt_error_handler(uint32_t nrf_error, void *p_ctx, uint16_t conn_handle)
{
	ble_ldi_c_t *p_ble_ldi_c = (ble_ldi_c_t *)p_ctx;

	NRF_LOG_DEBUG("A GATT Client error %d has occurred on conn_handle: 0X%X", nrf_error, conn_handle);

	if(p_ble_ldi_c->error_handler != NULL) {
		p_ble_ldi_c->error_handler(nrf_error);
	}
}

static bool decode_ldi_protobuf(ble_ldi_t *const p_dst, const uint8_t *const p_buf, size_t buf_len)
{
	ASSERT(p_dst);
	ASSERT(p_buf);
	if(!buf_len)
		return false;

	pb_istream_t stream = pb_istream_from_buffer(p_buf, buf_len);
	return pb_decode(&stream, com_bosch_ebike_LiveData_fields, p_dst);
}

static void on_hvx(ble_ldi_c_t *p_ble_ldi_c, const ble_evt_t *p_ble_evt)
{
	// Check if the event is on the link for this instance.
	if(p_ble_ldi_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle) {
		// not directed to this instance. Ignore.
		return;
	}

	NRF_LOG_DEBUG("Received HVX on link 0x%x, live_data_char_handle 0x%x", p_ble_evt->evt.gattc_evt.conn_handle,
				  p_ble_ldi_c->peer_ldi_db.live_data_char_handle);

	// Check if this is a Live Data Interface notification.
	if(p_ble_evt->evt.gattc_evt.params.hvx.handle == p_ble_ldi_c->peer_ldi_db.live_data_char_handle) {
		ble_ldi_c_evt_t ble_ldi_c_evt = {0};

		ble_ldi_c_evt.evt_type = BLE_LDI_C_EVT_LIVE_DATA_NOTIFICATION;
		ble_ldi_c_evt.conn_handle = p_ble_ldi_c->conn_handle;

		bool status = decode_ldi_protobuf(&ble_ldi_c_evt.params.ldi, p_ble_evt->evt.gattc_evt.params.hvx.data,
										  p_ble_evt->evt.gattc_evt.params.hvx.len);
		if(!status) {
			NRF_LOG_DEBUG("Decoding protobuf failed");
			if(p_ble_ldi_c->error_handler) {
				p_ble_ldi_c->error_handler(NRF_ERROR_INVALID_DATA);
			}
			return;
		}

		if(p_ble_ldi_c->evt_handler) {
			p_ble_ldi_c->evt_handler(p_ble_ldi_c, &ble_ldi_c_evt);
		}
	}
}

static void on_disconnected(ble_ldi_c_t *p_ble_ldi_c, const ble_evt_t *p_ble_evt)
{
	if(p_ble_ldi_c->conn_handle == p_ble_evt->evt.gap_evt.conn_handle) {
		p_ble_ldi_c->conn_handle = BLE_CONN_HANDLE_INVALID;
		p_ble_ldi_c->peer_ldi_db.live_data_cccd_handle = BLE_GATT_HANDLE_INVALID;
		p_ble_ldi_c->peer_ldi_db.live_data_char_handle = BLE_GATT_HANDLE_INVALID;

		ble_ldi_c_evt_t ble_ldi_c_evt = {0};

		ble_ldi_c_evt.evt_type = BLE_LDI_C_EVT_DISCONNECT;
		ble_ldi_c_evt.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

		if(p_ble_ldi_c->evt_handler) {
			p_ble_ldi_c->evt_handler(p_ble_ldi_c, &ble_ldi_c_evt);
		}
	}
}

static void on_read_rsp(ble_ldi_c_t *p_ble_ldi_c, ble_evt_t const *p_ble_evt)
{
	ble_ldi_c_evt_t ble_ldi_c_evt = {0};
	ble_gattc_evt_read_rsp_t const *p_response;

	p_response = &p_ble_evt->evt.gattc_evt.params.read_rsp;
	ble_ldi_c_evt.evt_type = BLE_LDI_C_EVT_LIVE_DATA_READ;
	ble_ldi_c_evt.conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;

	if(p_ble_ldi_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle) {
		// not directed to this instance. Ignore.
		return;
	}

	if(p_response->handle != p_ble_ldi_c->peer_ldi_db.live_data_char_handle) {
		NRF_LOG_DEBUG("Received READ_RSP on handle 0x%x, expected 0x%x. Ignore.", p_response->handle,
					  p_ble_ldi_c->peer_ldi_db.live_data_char_handle);
		return;
	}

	NRF_LOG_DEBUG("LDI READ_RSP on link 0x%x. Got %d bytes at offset %d", p_ble_evt->evt.gattc_evt.conn_handle, p_response->len,
				  p_response->offset);

	bool status = decode_ldi_protobuf(&ble_ldi_c_evt.params.ldi, p_response->data, p_response->len);
	if(!status) {
		NRF_LOG_DEBUG("Decoding protobuf failed");
		if(p_ble_ldi_c->error_handler) {
			p_ble_ldi_c->error_handler(NRF_ERROR_INVALID_DATA);
		}
		return;
	}

	if(p_ble_ldi_c->evt_handler) {
		p_ble_ldi_c->evt_handler(p_ble_ldi_c, &ble_ldi_c_evt);
	}
}

void ble_ldi_c_on_db_disc_evt(ble_ldi_c_t *p_ble_ldi_c, const ble_db_discovery_evt_t *p_evt)
{
	// Check if the Live Data Interface Service was discovered.
	if(p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE && p_evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_LDI_SERVICE &&
	   p_evt->params.discovered_db.srv_uuid.type == ble_ldi_c_get_base_type()) {

		// Find the CCCD Handle of the Live Data Interface characteristic.
		uint32_t i;

		ble_ldi_c_evt_t evt;
		bool ldi_char_found = false;

		evt.evt_type = BLE_LDI_C_EVT_DISCOVERY_COMPLETE;
		evt.conn_handle = p_evt->conn_handle;

		for(i = 0; i < p_evt->params.discovered_db.char_count; i++) {
			if(p_evt->params.discovered_db.charateristics[i].characteristic.uuid.uuid == BLE_UUID_LDI_CHAR) {
				// Found Live Data Interface characteristic. Store CCCD handle and break.
				evt.params.peer_db.live_data_cccd_handle = p_evt->params.discovered_db.charateristics[i].cccd_handle;
				evt.params.peer_db.live_data_char_handle =
					p_evt->params.discovered_db.charateristics[i].characteristic.handle_value;
				ldi_char_found = true;
				break;
			}
		}

		if(!ldi_char_found) {
			// Live Data Interface Characteristic is marked as optional for Live Data Interface Service
			NRF_LOG_DEBUG("LDI char missing from LDI service. Aborting");
			return;
		}

		NRF_LOG_DEBUG("Live Data Interface Service discovered at peer.");
		// If the instance has been assigned prior to db_discovery, assign the db_handles.
		if(p_ble_ldi_c->conn_handle != BLE_CONN_HANDLE_INVALID) {
			if((p_ble_ldi_c->peer_ldi_db.live_data_cccd_handle == BLE_GATT_HANDLE_INVALID) &&
			   (p_ble_ldi_c->peer_ldi_db.live_data_char_handle == BLE_GATT_HANDLE_INVALID)) {
				p_ble_ldi_c->peer_ldi_db = evt.params.peer_db;
			}
		}

		if(p_ble_ldi_c->evt_handler) {
			p_ble_ldi_c->evt_handler(p_ble_ldi_c, &evt);
		}
	}
}

ret_code_t ble_ldi_c_init(ble_ldi_c_t *const p_ble_ldi_c, ble_ldi_c_init_t *const p_ble_ldi_c_init)
{
	ASSERT(p_ble_ldi_c);
	ASSERT(p_ble_ldi_c_init);

	p_ble_ldi_c->evt_handler = p_ble_ldi_c_init->evt_handler;
	p_ble_ldi_c->error_handler = p_ble_ldi_c_init->error_handler;
	p_ble_ldi_c->p_gatt_queue = p_ble_ldi_c_init->p_gatt_queue;
	p_ble_ldi_c->conn_handle = BLE_CONN_HANDLE_INVALID;
	p_ble_ldi_c->peer_ldi_db.live_data_cccd_handle = BLE_GATT_HANDLE_INVALID;
	p_ble_ldi_c->peer_ldi_db.live_data_char_handle = BLE_GATT_HANDLE_INVALID;

	ble_uuid_t ldi_uuid = {.uuid = BLE_UUID_LDI_SERVICE, .type = ble_ldi_c_get_base_type()};

	return ble_db_discovery_evt_register(&ldi_uuid);
}

void ble_ldi_c_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ble_ldi_c_t *p_ble_ldi_c = (ble_ldi_c_t *)p_context;

	if((p_ble_ldi_c == NULL) || (p_ble_evt == NULL)) {
		return;
	}

	switch(p_ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(p_ble_ldi_c, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(p_ble_ldi_c, p_ble_evt);
		break;

	case BLE_GATTC_EVT_READ_RSP:
		on_read_rsp(p_ble_ldi_c, p_ble_evt);
		break;

	default:
		break;
	}
}

static ret_code_t cccd_configure(ble_ldi_c_t *p_ble_ldi_c, bool enable)
{
	NRF_LOG_DEBUG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d", p_ble_ldi_c->peer_ldi_db.live_data_cccd_handle,
				  p_ble_ldi_c->conn_handle);

	nrf_ble_gq_req_t ldi_c_req;
	uint8_t cccd[BLE_CCCD_VALUE_LEN];
	uint16_t cccd_val = enable ? BLE_GATT_HVX_NOTIFICATION : 0;

	cccd[0] = LSB_16(cccd_val);
	cccd[1] = MSB_16(cccd_val);

	memset(&ldi_c_req, 0, sizeof(ldi_c_req));

	ldi_c_req.type = NRF_BLE_GQ_REQ_GATTC_WRITE;
	ldi_c_req.error_handler.cb = gatt_error_handler;
	ldi_c_req.error_handler.p_ctx = p_ble_ldi_c;
	ldi_c_req.params.gattc_write.handle = p_ble_ldi_c->peer_ldi_db.live_data_cccd_handle;
	ldi_c_req.params.gattc_write.len = BLE_CCCD_VALUE_LEN;
	ldi_c_req.params.gattc_write.p_value = cccd;
	ldi_c_req.params.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ;

	return nrf_ble_gq_item_add(p_ble_ldi_c->p_gatt_queue, &ldi_c_req, p_ble_ldi_c->conn_handle);
}

ret_code_t ble_ldi_c_ldi_notif_enable(ble_ldi_c_t *const p_ble_ldi_c)
{
	ASSERT(p_ble_ldi_c);

	return cccd_configure(p_ble_ldi_c, true);
}

ret_code_t ble_ldi_c_ldi_read(ble_ldi_c_t *const p_ble_ldi_c)
{
	ASSERT(p_ble_ldi_c);

	nrf_ble_gq_req_t ldi_c_req;
	memset(&ldi_c_req, 0, sizeof(ldi_c_req));

	ldi_c_req.type = NRF_BLE_GQ_REQ_GATTC_READ;
	ldi_c_req.error_handler.cb = gatt_error_handler;
	ldi_c_req.error_handler.p_ctx = p_ble_ldi_c;
	ldi_c_req.params.gattc_read.handle = p_ble_ldi_c->peer_ldi_db.live_data_char_handle;
	ldi_c_req.params.gattc_read.offset = 0;

	return nrf_ble_gq_item_add(p_ble_ldi_c->p_gatt_queue, &ldi_c_req, p_ble_ldi_c->conn_handle);
}

ret_code_t ble_ldi_c_handles_assign(ble_ldi_c_t *const p_ble_ldi_c, uint16_t conn_handle,
									const ldi_db_t *const p_peer_ldi_handles)
{
	ASSERT(p_ble_ldi_c);

	p_ble_ldi_c->conn_handle = conn_handle;
	if(p_peer_ldi_handles != NULL) {
		p_ble_ldi_c->peer_ldi_db = *p_peer_ldi_handles;
	}

	return nrf_ble_gq_conn_handle_register(p_ble_ldi_c->p_gatt_queue, conn_handle);
}
