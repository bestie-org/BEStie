#ifndef _BLE_CSC_H__
#define _BLE_CSC_H__

/**
 * Multiperipheral capable version of BLE Cycling Speed and Cadence Service
 * Only mandatory parts are implemented.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "ble.h"
#include "ble_sensor_location.h"
#include "ble_srv_common.h"
#include "nrf_ble_gq.h"
#include "nrf_sdh_ble.h"
#include "sdk_config.h"

#ifndef BLE_CSC_BLE_OBSERVER_PRIO
#define BLE_CSC_BLE_OBSERVER_PRIO 2
#endif

// maximum length of Cycling Speed and Cadence notification
#define BLE_CSC_MAX_SC_MEAS_LEN 11

// Cycling Power Measurement flags
#define BLE_CSC_FEATURE_WHEEL_REVOLUTION_DATA_BIT (1 << 0)
#define BLE_CSC_FEATURE_CRANK_REVOLUTION_DATA_BIT (1 << 1)

/**@brief   Macro for defining a ble_csc_t instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CSC_DEF(_name)  \
	static ble_csc_t _name; \
	NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_CSC_BLE_OBSERVER_PRIO, ble_csc_on_ble_evt, &_name)

typedef struct
{
	uint16_t service_handle;
	ble_gatts_char_handles_t sc_measurement_handles;
	ble_gatts_char_handles_t sc_feature_handles;
	ble_gatts_char_handles_t sensor_location_handles;
	ble_gatts_char_handles_t sc_control_point_handles;
	nrf_ble_gq_t *p_gatt_queue;
	bool sc_measurement_notification_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	bool sc_control_point_indication_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	uint16_t features;
} ble_csc_t;

typedef struct
{
	security_req_t sc_measurement_cccd_access;

	security_req_t sc_feature_read_access;

	security_req_t sensor_location_read_access;

	security_req_t sc_control_point_write_access;
	security_req_t sc_control_point_cccd_access;

	ble_sensor_location_t sensor_location;
	nrf_ble_gq_t *p_gatt_queue;
	uint16_t features;
} ble_csc_init_t;

typedef struct
{
	uint32_t total_wheel_revolutions;
	uint16_t last_wheel_event;
	bool speed_present;

	uint16_t total_crank_revolutions;
	uint16_t last_crank_event;
	bool cadence_present;
} ble_csc_data_t;

/**@brief     Function for initializing the Cycling Speed and Cadence Service module.
 *
 * @param[in]   p_csc       Pointer to the ble_csc_t instance.
 * @param[in]   p_csc_init  Pointer to the initialization structure.
 *
 * @retval     NRF_SUCCESS               If initialization was successful.
 * @retval     NRF_ERROR_INVALID_PARAM   If the init structure is invalid.
 */
ret_code_t ble_csc_init(ble_csc_t *const p_csc, const ble_csc_init_t *const p_csc_init);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @param[in]   p_ble_evt  Bluetooth stack event.
 * @param[in]   p_context  Pointer to the ble_csc_t instance.
 */
void ble_csc_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief     Function for sending Cycling Speed and Cadence Measurement notification.
 *
 * @param[in]   p_csc   Pointer to the ble_csc_t instance.
 * @param[in]   p_data  Pointer to the measurement data.
 *
 * @retval     NRF_SUCCESS   If the notification was queued successfully.
 */
ret_code_t ble_csc_measurement_update(ble_csc_t *const p_csc, const ble_csc_data_t *const p_data);

#endif
