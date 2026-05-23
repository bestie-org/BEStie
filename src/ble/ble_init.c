#include "ble_init.h"
#include "app_error.h"
#include "app_timer.h"
#include "ble_conn_params.h"
#include "nrf_ble_gatt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "uuid.h"

#define NRF_LOG_MODULE_NAME ble_init
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

NRF_BLE_GATT_DEF(m_gatt);

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);

// register a handler for BLE events.
NRF_SDH_BLE_OBSERVER(m_ble_observer, CFG_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
	ret_code_t err_code = NRF_SUCCESS;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_DISCONNECTED:
		NRF_LOG_DEBUG("Disconnected.");
		break;

	case BLE_GAP_EVT_CONNECTED: {
		NRF_LOG_DEBUG("Connected.");

// request PHY update if application config demands it
#if defined(CFG_BLE_PHY) && CFG_BLE_PHY != BLE_GAP_PHY_1MBPS
		ble_gap_phys_t const phys = {
			.rx_phys = CFG_BLE_PHY,
			.tx_phys = CFG_BLE_PHY,
		};
		err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
		APP_ERROR_CHECK(err_code);
#endif
	} break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST: // accept central PHY settings
	{
		NRF_LOG_DEBUG("PHY update request.");
		ble_gap_phys_t const phys = {
			.rx_phys = BLE_GAP_PHY_AUTO,
			.tx_phys = BLE_GAP_PHY_AUTO,
		};
		err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
		APP_ERROR_CHECK(err_code);
	} break;

	case BLE_GAP_EVT_PHY_UPDATE: {
		NRF_LOG_DEBUG("PHY update status:%d, RX:%d, TX:%d",
					  p_ble_evt->evt.gap_evt.params.phy_update.status,
					  p_ble_evt->evt.gap_evt.params.phy_update.rx_phy,
					  p_ble_evt->evt.gap_evt.params.phy_update.tx_phy);
	} break;

	case BLE_GATTC_EVT_TIMEOUT:
		// Disconnect on GATT Client timeout event.
		NRF_LOG_DEBUG("GATT Client Timeout.");
		err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
										 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		APP_ERROR_CHECK(err_code);
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		// Disconnect on GATT Server timeout event.
		NRF_LOG_DEBUG("GATT Server Timeout.");
		err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
										 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		APP_ERROR_CHECK(err_code);
		break;

	default:
		// No implementation needed.
		break;
	}
}

static void softdevice_init(void)
{
	ret_code_t err_code;

	err_code = nrf_sdh_enable_request();
	APP_ERROR_CHECK(err_code);

	// Configure the BLE stack using the default settings.
	// Fetch the start address of the application RAM.
	uint32_t ram_start = 0;
	err_code = nrf_sdh_ble_default_cfg_set(CFG_BLE_CONN_CFG_TAG, &ram_start);
	APP_ERROR_CHECK(err_code);

	// Enable BLE stack.
	err_code = nrf_sdh_ble_enable(&ram_start);
	APP_ERROR_CHECK(err_code);
}

static void gap_params_init(void)
{
	ret_code_t err_code;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)CFG_DEVICE_NAME,
										  strlen(CFG_DEVICE_NAME));
	APP_ERROR_CHECK(err_code);

	err_code = sd_ble_gap_appearance_set(CFG_DEVICE_APPEARANCE);
    APP_ERROR_CHECK(err_code);

	ble_gap_conn_params_t gap_conn_params = {
		.min_conn_interval = MSEC_TO_UNITS(CFG_MIN_CONN_INTERVAL, UNIT_1_25_MS),
		.max_conn_interval = MSEC_TO_UNITS(CFG_MAX_CONN_INTERVAL, UNIT_1_25_MS),
		.slave_latency = CFG_SLAVE_LATENCY,
		.conn_sup_timeout = MSEC_TO_UNITS(CFG_CONN_SUP_TIMEOUT, UNIT_10_MS)};

	err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
	APP_ERROR_CHECK(err_code);
}

static void gatt_init(void)
{
	ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
	APP_ERROR_CHECK(err_code);
}

static void conn_params_init(void)
{
#if NRF_BLE_CONN_PARAMS_ENABLED

	ret_code_t err_code;

	ble_conn_params_init_t cp_init = {
		.first_conn_params_update_delay = APP_TIMER_TICKS(CFG_CONN_PARAM_FIRST_UPDATE_DELAY),
		.next_conn_params_update_delay = APP_TIMER_TICKS(CFG_CONN_PARAM_NEXT_UPDATE_DELAY),
		.max_conn_params_update_count = CFG_CONN_PARAM_MAX_UPDATE_COUNT,
		.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID,
		.disconnect_on_fail = CFG_CONN_PARAM_DISCONNECT_ON_FAIL};
	err_code = ble_conn_params_init(&cp_init);
	APP_ERROR_CHECK(err_code);

#endif
}

static void peer_manager_evt_handler(pm_evt_t const *p_evt)
{
	pm_handler_on_pm_evt(p_evt);
	pm_handler_flash_clean(p_evt);
}

static void peer_manager_init(bool erase_bonds)
{
	// security parameters used by all connections
	ble_gap_sec_params_t sec_param = {.bond = CFG_SEC_PARAM_BOND,
									  .mitm = CFG_SEC_PARAM_MITM,
									  .lesc = CFG_SEC_PARAM_LESC,
									  .keypress = CFG_SEC_PARAM_KEYPRESS,
									  .io_caps = CFG_SEC_PARAM_IO_CAPABILITIES,
									  .oob = CFG_SEC_PARAM_OOB,
									  .min_key_size = CFG_SEC_PARAM_MIN_KEY_SIZE,
									  .max_key_size = CFG_SEC_PARAM_MAX_KEY_SIZE,
									  .kdist_own.enc = (CFG_SEC_PARAM_BOND) ? 1 : 0,
									  .kdist_own.id = (CFG_SEC_PARAM_BOND) ? 1 : 0,
									  .kdist_peer.enc = (CFG_SEC_PARAM_BOND) ? 1 : 0,
									  .kdist_peer.id = (CFG_SEC_PARAM_BOND) ? 1 : 0};

	ret_code_t err_code;

	err_code = pm_init();
	APP_ERROR_CHECK(err_code);

	if(erase_bonds) {
		pm_peers_delete();
	}

	err_code = pm_sec_params_set(&sec_param);
	APP_ERROR_CHECK(err_code);

	err_code = pm_register(peer_manager_evt_handler);
	APP_ERROR_CHECK(err_code);
}

// initalize BLE platform
void ble_init(void)
{
	softdevice_init();
	gap_params_init();
	gatt_init();
	conn_params_init();
	peer_manager_init(false);
	uuid_init();
}

// return handle to nRF BLE GATT module
nrf_ble_gatt_t *ble_nrf_gatt_get(void)
{
	return &m_gatt;
}
