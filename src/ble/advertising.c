#include "advertising.h"
#include "ble_advdata.h"
#include "ble_conn_state.h"
#include "ble_cpms.h"
#include "ble_ftms.h"
#include "ble_ldi_c.h"
#include "ebike_state_mgr.h"
#include "nrf_sdh_ble.h"
#include "sdk_config.h"

#define NRF_LOG_MODULE_NAME advertising
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static void advertising_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);
NRF_SDH_BLE_OBSERVER(advertising_ble_obs, 1, advertising_on_ble_evt, NULL);

/**
 * @brief Multi slot advertiser
 *
 * Softdevice does not support multiple advertising sets. Many fitness devices do not process scan responses and having 128bit
 * solicited service UUID for LDI leaves almost no room for anything else in main advertising packet. We need to be crafty.
 *
 * This module switches between multiple advertising payloads on regular basis.
 * Each set can have its own advertising parameters.
 *
 * Data are encoded into advertising buffers on advertising_init() so cost of switching sets is a simple pointer swap compared to
 * full re-encoding everything when using SDK ble_advertising module
 */

/**
 * Flow app is really finnicky during 'add component' device scan. Intervals longer than 250ms result in pairing failure in my
 * testing.
 *
 * In order to save power fast advertising is applied from power on to first successful LDI peer connection.
 * After that relatively short but more power optimized interval is used.
 *
 */
#define ADVERTISING_INTERVAL_LDI_MS			500
#define ADVERTISING_INTERVAL_LDI_PAIRING_MS 250

/**
 * Garmin devices find sensor to add as long as they get a single advertising packet within ~15s scan window
 *
 * For reconnection shorter intervals work better. So in order to save some power there will be fast advertising scheme when
 * bike is connected and longer interval otherwise.
 *
 */
#define ADVERTISING_INTERVAL_FTMS_CPMS_MS				 1000
#define ADVERTISING_INTERVAL_FTMS_CPMS_BIKE_CONNECTED_MS (ADVERTISING_INTERVAL_FTMS_CPMS_MS / 2)

typedef struct
{
	const ble_advdata_t adv_data;
	ble_data_t adv_buff;
	uint8_t advdata_raw[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

	/**
	 * set to true to encode and use scan response data for this set
	 *
	 * note: if you need scan responses you have to enable and provide some data for them on **all** advertising sets. Otherwise
	 * all advertising timings go haywire!
	 *
	 * This is a potential softdevice bug / undocumented behavior
	 *
	 */
	const bool scan_resp_enabled;
	const ble_advdata_t scan_resp_data;
	ble_data_t scan_resp_buff;
	uint8_t scan_resp_raw[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

	ble_gap_adv_params_t params;
} advertising_data_t;

static ble_uuid_t ldi_solicited_services_uuids[] = {{BLE_UUID_LDI_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}};

#if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED

static ble_uuid_t ftms_advertised_services_uuids[] = {
	{BLE_UUID_FTMS_SERVICE, BLE_UUID_TYPE_BLE},
};

/**
 * Fitness Machine Service advertising data. Machine available, indoor bike type
 * This is required for some Garmin devices to detect BEStie
 */
static uint8_t ftms_service_data[] = {0x01, 0x20, 0x00};
static ble_advdata_service_data_t ftms_service_data_array = {
	.service_uuid = BLE_UUID_FTMS_SERVICE,
	.data = {.p_data = ftms_service_data, .size = sizeof(ftms_service_data) / sizeof(ftms_service_data[0])}};

#endif

#if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED

static ble_uuid_t cpms_advertised_services_uuids[] = {
	{BLE_UUID_CYCLING_SPEED_AND_CADENCE, BLE_UUID_TYPE_BLE},
	{BLE_UUID_CYCLING_POWER_SERVICE, BLE_UUID_TYPE_BLE},
};

#endif

static advertising_data_t advertising_data[] = {
	// Bosch LDI peer
	{
		.adv_data =
			{
				.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
				.name_type = BLE_ADVDATA_FULL_NAME,
				.uuids_solicited = {.uuid_cnt = sizeof(ldi_solicited_services_uuids) / sizeof(ldi_solicited_services_uuids[0]),
									.p_uuids = ldi_solicited_services_uuids},
				// no BLE appearance, there's not even a single byte of space left
			},
		.scan_resp_enabled = true,
		.scan_resp_data =
			{
				// flags are not allowed in scan response. They mirror main advertising packet
				.name_type = BLE_ADVDATA_FULL_NAME, /* Bosch Flow app shows device icon on 'add component' screen only when both
													   appearance and name are present in scan response.*/
				.include_appearance = true,
			},
		.params =
			{
				.primary_phy = BLE_GAP_PHY_1MBPS,
				.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
				.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL_LDI_PAIRING_MS, UNIT_0_625_MS),

				/**
				 * this is rough empirical correlation.
				 *
				 * For Flow app 'add component' scan to reliably succeed there's minimum real time window
				 * LDI peer advertising must remain active.
				 *
				 * On the other hand we don't want huge advertising time imbalance with single fitness service active.
				 */
				.duration = MSEC_TO_UNITS(4000 + ((CFG_FTMS_ENABLED) ? 1 : 0) * 2500 + ((CFG_CPMS_CSC_ENABLED) ? 1 : 0) * 2500,
										  UNIT_10_MS),
			},
	},
#if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED
	// FTMS Appliance for modern devices
	{
		.adv_data =
			{
				.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
				.name_type = BLE_ADVDATA_FULL_NAME,
				.uuids_more_available =
					{
						.p_uuids = ftms_advertised_services_uuids,
						.uuid_cnt = sizeof(ftms_advertised_services_uuids) / sizeof(ftms_advertised_services_uuids[0]),
					},
				.p_service_data_array = &ftms_service_data_array,
				.service_data_count = 1,
			},
		.scan_resp_enabled = true,
		.scan_resp_data =
			{
				// flags are not allowed in scan response. They mirror main advertising packet
				.include_appearance = true,
			},
		.params =
			{
				.primary_phy = BLE_GAP_PHY_1MBPS,
				.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
				.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL_FTMS_CPMS_MS, UNIT_0_625_MS),
				.duration = MSEC_TO_UNITS(4000, UNIT_10_MS),
			},
	},
#endif // #if defined (CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED

#if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED
	// CSC / CPMS Appliance for legacy devices,
	{
		.adv_data =
			{
				.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
				.name_type = BLE_ADVDATA_FULL_NAME,
				.uuids_more_available =
					{
						.p_uuids = cpms_advertised_services_uuids,
						.uuid_cnt = sizeof(cpms_advertised_services_uuids) / sizeof(cpms_advertised_services_uuids[0]),
					},
			},
		.scan_resp_enabled = true,
		.scan_resp_data =
			{
				// flags are not allowed in scan response. They mirror main advertising packet
				.include_appearance = true,
			},
		.params =
			{
				.primary_phy = BLE_GAP_PHY_1MBPS,
				.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
				.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL_FTMS_CPMS_MS, UNIT_0_625_MS),
				.duration = MSEC_TO_UNITS(2000, UNIT_10_MS),
			},
	},
#endif // #if defined (CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED
};

// no configurable parts beyond this point

static const size_t advertising_data_len = sizeof(advertising_data) / sizeof(advertising_data[0]);

static const advertising_data_t *const p_advertising_data_ldi = &advertising_data[0];

static const advertising_data_t *p_advertising_data_current = &advertising_data[0];

static uint8_t advertising_set_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

static const advertising_data_t *advertising_set_next(void)
{
	p_advertising_data_current += 1;
	if(p_advertising_data_current >= advertising_data + advertising_data_len) {
		p_advertising_data_current = &advertising_data[0];
	}

	return p_advertising_data_current;
}

static ret_code_t advertising_set_update(void)
{
	ret_code_t err_code = NRF_SUCCESS;
	uint32_t periph_link_cnt = ble_conn_state_peripheral_conn_count();

	if(periph_link_cnt >= NRF_SDH_BLE_PERIPHERAL_LINK_COUNT) {
		return NRF_SUCCESS;
	}

	const advertising_data_t *p_next_advertising_set = advertising_set_next();

	if(ebike_is_connected()) {
		// LDI peer is the only advertising set. Enforce one ebike connection at a time.
		if(sizeof(advertising_data) / sizeof(advertising_data[0]) < 2) {
			return NRF_SUCCESS;
		}

		// skip advertising LDI peer when ebike is connected and other advertising sets exist
		if(p_next_advertising_set == p_advertising_data_ldi) {
			p_next_advertising_set = advertising_set_next();
		}
	}

	ble_gap_adv_data_t adv_data = {
		.adv_data.p_data = p_next_advertising_set->adv_buff.p_data,
		.adv_data.len = p_next_advertising_set->adv_buff.len,
		.scan_rsp_data.p_data = p_next_advertising_set->scan_resp_buff.p_data, // NULL data and zero len are ok for scan response
		.scan_rsp_data.len = p_next_advertising_set->scan_resp_buff.len,
	};

	err_code = sd_ble_gap_adv_set_configure(&advertising_set_handle, &adv_data, &p_next_advertising_set->params);
	VERIFY_SUCCESS(err_code);

	err_code = advertising_start();
	return err_code;
}

static void advertising_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
	ret_code_t err_code;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		err_code = advertising_start();
		if((err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_NO_MEM)) {
			APP_ERROR_CHECK(err_code);
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		err_code = advertising_start();
		if((err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_NO_MEM)) {
			APP_ERROR_CHECK(err_code);
		}
		break;

	case BLE_GAP_EVT_ADV_SET_TERMINATED:
		if(p_ble_evt->evt.gap_evt.params.adv_set_terminated.reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_TIMEOUT ||
		   p_ble_evt->evt.gap_evt.params.adv_set_terminated.reason == BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_LIMIT_REACHED) {
			err_code = advertising_set_update();
			if(err_code != NRF_ERROR_INVALID_STATE) {
				APP_ERROR_CHECK(err_code);
			}
		}
		break;

	default:
		break;
	}
}

void advertising_init(void)
{
	ret_code_t err_code;

	// replace placeholder 128 UUID base with a proper one
	ldi_solicited_services_uuids[0].type = ble_ldi_c_get_base_type();

	for(size_t i = 0; i < advertising_data_len; i++) {
		advertising_data_t *const advertising_set = &advertising_data[i];

		advertising_set->adv_buff.p_data = advertising_set->advdata_raw;
		advertising_set->adv_buff.len = sizeof(advertising_set->advdata_raw);

		err_code =
			ble_advdata_encode(&advertising_set->adv_data, advertising_set->adv_buff.p_data, &advertising_set->adv_buff.len);
		APP_ERROR_CHECK(err_code);

		if(advertising_set->scan_resp_enabled) {
			advertising_set->scan_resp_buff.p_data = advertising_set->scan_resp_raw;
			advertising_set->scan_resp_buff.len = sizeof(advertising_set->scan_resp_raw);

			err_code = ble_advdata_encode(&advertising_set->scan_resp_data, advertising_set->scan_resp_buff.p_data,
										  &advertising_set->scan_resp_buff.len);
			APP_ERROR_CHECK(err_code);
		} else {
			advertising_set->scan_resp_buff.p_data = NULL;
			advertising_set->scan_resp_buff.len = 0;
		}
	}

	ble_gap_adv_data_t adv_data = {
		.adv_data.p_data = p_advertising_data_current->adv_buff.p_data,
		.adv_data.len = p_advertising_data_current->adv_buff.len,
		.scan_rsp_data.p_data =
			p_advertising_data_current->scan_resp_buff.p_data, // NULL data and zero len are ok for scan response
		.scan_rsp_data.len = p_advertising_data_current->scan_resp_buff.len,
	};

	err_code = sd_ble_gap_adv_set_configure(&advertising_set_handle, &adv_data, &p_advertising_data_current->params);
	APP_ERROR_CHECK(err_code);
}

ret_code_t advertising_start(void)
{
	uint32_t periph_link_cnt = ble_conn_state_peripheral_conn_count();

	if(periph_link_cnt >= NRF_SDH_BLE_PERIPHERAL_LINK_COUNT) {
		return NRF_ERROR_NO_MEM;
	}

	return sd_ble_gap_adv_start(advertising_set_handle, CFG_BLE_CONN_CFG_TAG);
}

void advertising_signal_ebike_connected_state(const bool ebike_connected)
{
	ret_code_t err_code = NRF_SUCCESS;

	err_code = sd_ble_gap_adv_stop(advertising_set_handle);
	if(err_code != NRF_ERROR_INVALID_STATE) {
		APP_ERROR_CHECK(err_code);
	}

	for(size_t i = 0; i < sizeof(advertising_data) / sizeof(*advertising_data); i++) {

		if(&advertising_data[i] == p_advertising_data_ldi) {

			// switch to regular advertising interval for LDI peer
			if(ebike_connected) {
				advertising_data[i].params.interval = MSEC_TO_UNITS(ADVERTISING_INTERVAL_LDI_MS, UNIT_0_625_MS);
			}

			continue;
		}

		// fast advertising path for fitness peripherals
		advertising_data[i].params.interval = (ebike_connected)
												  ? MSEC_TO_UNITS(ADVERTISING_INTERVAL_FTMS_CPMS_BIKE_CONNECTED_MS, UNIT_0_625_MS)
												  : MSEC_TO_UNITS(ADVERTISING_INTERVAL_FTMS_CPMS_MS, UNIT_0_625_MS);
	}

	if((ebike_connected && p_advertising_data_current == p_advertising_data_ldi) ||
	   (!ebike_connected && p_advertising_data_current != p_advertising_data_ldi)) {

		p_advertising_data_current = (ebike_connected) ? advertising_set_next() : p_advertising_data_ldi;

		ble_gap_adv_data_t adv_data = {
			.adv_data.p_data = p_advertising_data_current->adv_buff.p_data,
			.adv_data.len = p_advertising_data_current->adv_buff.len,
			.scan_rsp_data.p_data =
				p_advertising_data_current->scan_resp_buff.p_data, // NULL data and zero len are ok for scan response
			.scan_rsp_data.len = p_advertising_data_current->scan_resp_buff.len,
		};

		err_code = sd_ble_gap_adv_set_configure(&advertising_set_handle, &adv_data, &p_advertising_data_current->params);
		APP_ERROR_CHECK(err_code);
	}

	err_code = advertising_start();
	if(err_code != NRF_ERROR_NO_MEM && err_code != NRF_ERROR_INVALID_STATE) {
		APP_ERROR_CHECK(err_code);
	}
}
