#include "services.h"
#include "ble_conn_params.h"
#include "ble_cpms.h"
#include "ble_csc.h"
#include "ble_db_discovery.h"
#include "ble_ftms.h"
#include "ble_ldi_c.h"
#include "ble_srv_common.h"
#include "csc_state_handler.h"
#include "ebike_advertising_state_handler.h"
#include "ebike_live_data.pb.h"
#include "ebike_state_mgr.h"
#include "ftms_state_handler.h"
#include "nrf_ble_gq.h"
#include "sdk_config.h"

#define NRF_LOG_MODULE_NAME services
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

/**
 * Allocate memory to service GATT queues
 *
 * LDI client is the only spot where we need to be smart.
 *
 * ldi_gatt_queue is used to handle incoming LDI notification that is huge
 *
 * but it's also used for connection setup so it needs to handle a few (1-2)
 * incoming notifications that are COM_BOSCH_EBIKE_EBIKE_LIVE_DATA_PB_H_MAX_SIZE long
 * (70 bytes in initial LDI spec) and a lot of smaller ones
 *
 * for that reason element pool size will be the size of standard ATT MTU - 23 bytes
 * but there will be enough elements to allocate into 3 LDI notifications
 * so 3*70 bytes of space split into 10 chunks 23 bytes each
 */
NRF_BLE_GQ_CUSTOM_DEF(ldi_gatt_queue_inst, NRF_SDH_BLE_PERIPHERAL_LINK_COUNT, /* _max_connections */
					  NRF_SDH_BLE_PERIPHERAL_LINK_COUNT * 3,				  /* _queue_size */
					  BLE_GATT_ATT_MTU_DEFAULT,								  /* _pool_elem_size */
					  NRF_SDH_BLE_PERIPHERAL_LINK_COUNT *(
						  COM_BOSCH_EBIKE_EBIKE_LIVE_DATA_PB_H_MAX_SIZE / BLE_GATT_ATT_MTU_DEFAULT + 1) /* _pool_elem_count */
);

BLE_DB_DISCOVERY_ARRAY_DEF(db_disc_inst, NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);
BLE_LDI_C_ARRAY_DEF(ldi_inst, NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

/* For the rest of services we're only sending notifications so math is simple:
 *
 * mempool size: ATT MTU size
 * mempool element count should be: max number of peripheral connections * max number of outgoing notifications * number of ATT
 * MTU long chunks that fit the longest notification for this service
 */
#define SERVICE_GQ_DEF(_name, _max_notif_count_per_connection, _max_notif_len)                                          \
	NRF_BLE_GQ_CUSTOM_DEF(_name, NRF_SDH_BLE_PERIPHERAL_LINK_COUNT,								 /* _max_connections */ \
						  (_max_notif_count_per_connection * NRF_SDH_BLE_PERIPHERAL_LINK_COUNT), /* _queue_size */      \
						  BLE_GATT_ATT_MTU_DEFAULT,												 /* _pool_elem_size */  \
						  NRF_SDH_BLE_PERIPHERAL_LINK_COUNT * _max_notif_count_per_connection *                         \
							  (_max_notif_len / BLE_GATT_ATT_MTU_DEFAULT + 1) /* _pool_elem_count */                    \
	)

#if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED

SERVICE_GQ_DEF(cpms_gatt_queue_inst, 2, BLE_CPMS_MAX_SC_MEAS_LEN);
BLE_CPMS_DEF(cpms_inst);

SERVICE_GQ_DEF(csc_gatt_queue_inst, 2, BLE_CSC_MAX_SC_MEAS_LEN);
BLE_CSC_DEF(csc_inst);

#endif // #if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED

#if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED

SERVICE_GQ_DEF(ftms_gatt_queue_inst, 6, BLE_FTMS_MAX_INDOOR_BIKE_DATA_LEN);
BLE_FTMS_DEF(ftms_inst);

#endif // #if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED

SERVICE_GQ_DEF(bas_gatt_queue_inst, 1, BLE_BAS_MAX_BATTERY_LEVEL_DATA_LEN);
BLE_BAS_DEF(bas_inst);

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);
NRF_SDH_BLE_OBSERVER(ble_observer_inst, CFG_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
	ret_code_t err_code;

	switch(p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED: {
		APP_ERROR_CHECK_BOOL(p_ble_evt->evt.gap_evt.conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

		err_code = ble_db_discovery_start(&db_disc_inst[p_ble_evt->evt.gap_evt.conn_handle], p_ble_evt->evt.gap_evt.conn_handle);
		APP_ERROR_CHECK(err_code);
		break;
	}
	default:
		break;
	}
}

static void db_disc_handler(ble_db_discovery_evt_t *p_evt)
{
	APP_ERROR_CHECK_BOOL(p_evt->conn_handle < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT);

	ble_ldi_c_on_db_disc_evt(&ldi_inst[p_evt->conn_handle], p_evt);
}

static void db_discovery_init(void)
{
	ble_db_discovery_init_t db_init = {0};

	db_init.evt_handler = db_disc_handler;
	db_init.p_gatt_queue = &ldi_gatt_queue_inst;

	ret_code_t err_code = ble_db_discovery_init(&db_init);
	APP_ERROR_CHECK(err_code);
}

static void ldi_c_init(void)
{
	ret_code_t err_code;
	ble_ldi_c_init_t ldi_c_init_obj = {0};

	ldi_c_init_obj.evt_handler = ebike_state_on_ldi_c_evt;
	ldi_c_init_obj.p_gatt_queue = &ldi_gatt_queue_inst;

	for(uint8_t i = 0; i < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT; i++) {
		err_code = ble_ldi_c_init(&ldi_inst[i], &ldi_c_init_obj);
		APP_ERROR_CHECK(err_code);
	}
}

#if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED

ble_cpms_t *services_cpms_inst_get(void)
{
	return &cpms_inst;
}

ble_csc_t *services_csc_inst_get(void)
{
	return &csc_inst;
}

static void cpms_init(void)
{
	ret_code_t err_code;
	ble_cpms_init_t cpms_init = {.features =
									 BLE_CPMS_FEATURE_WHEEL_REVOLUTION_DATA_BIT | BLE_CPMS_FEATURE_CRANK_REVOLUTION_DATA_BIT,
								 .sensor_location = BLE_SENSOR_LOCATION_LEFT_CRANK,
								 .cycling_power_feature_read_access = SEC_OPEN,
								 .cycling_power_measurement_cccd_write_access = SEC_OPEN,
								 .cycling_power_sensor_location_read_access = SEC_OPEN,
								 .cycling_power_control_point_cccd_access = SEC_OPEN,
								 .cycling_power_control_point_write_access = SEC_OPEN,
								 .p_gatt_queue = &cpms_gatt_queue_inst};

	err_code = ble_cpms_init(&cpms_inst, &cpms_init);
	APP_ERROR_CHECK(err_code);
}

static void csc_init(void)
{
	ret_code_t err_code;
	ble_csc_init_t csc_init = {.features = BLE_CSC_FEATURE_WHEEL_REVOLUTION_DATA_BIT | BLE_CSC_FEATURE_CRANK_REVOLUTION_DATA_BIT,
							   .sc_measurement_cccd_access = SEC_OPEN,
							   .sensor_location = BLE_SENSOR_LOCATION_LEFT_CRANK,
							   .sc_feature_read_access = SEC_OPEN,
							   .sensor_location_read_access = SEC_OPEN,
							   .sc_control_point_write_access = SEC_OPEN,
							   .sc_control_point_cccd_access = SEC_OPEN,
							   .p_gatt_queue = &csc_gatt_queue_inst};

	err_code = ble_csc_init(&csc_inst, &csc_init);
	APP_ERROR_CHECK(err_code);
}
#endif // #if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED

#if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED
static void ftms_init(void)
{
	ret_code_t err_code;
	ble_ftms_init_t ftms_init = {.ftms_feature_read_access = SEC_OPEN,
								 .indoor_bike_data_cccd_access = SEC_OPEN,
								 .ftms_control_point_write_access = SEC_OPEN,
								 .ftms_control_point_cccd_access = SEC_OPEN,
								 .ftms_status_cccd_access = SEC_OPEN,
								 .p_gatt_queue = &ftms_gatt_queue_inst};

	err_code = ble_ftms_init(&ftms_inst, &ftms_init);
	APP_ERROR_CHECK(err_code);
}

ble_ftms_t *services_ftms_inst_get(void)
{
	return &ftms_inst;
}

#endif // #if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED

static void bas_init(void)
{
	ret_code_t err_code;
	ble_bas_init_t bas_init = {
		.battery_level_cccd_access = SEC_OPEN, .battery_level_read_access = SEC_OPEN, .p_gatt_queue = &bas_gatt_queue_inst};

	err_code = ble_bas_init(&bas_inst, &bas_init);
	APP_ERROR_CHECK(err_code);
}

ble_bas_t *services_bas_inst_get(void)
{
	return &bas_inst;
}

void services_init(void)
{
#if defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED
	cpms_init();
	csc_init();
	csc_state_handler_init();
#endif

#if defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED
	ftms_init();
	ftms_state_handler_init();
#endif

	bas_init();

	ebike_advertising_state_handler_init();

	db_discovery_init();
	ldi_c_init();
}
