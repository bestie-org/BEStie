#ifndef _BLE_CPMS_H__
#define _BLE_CPMS_H__

/**
 * Multiperipheral capable version of BLE Cycling Power Service
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

#ifndef BLE_CPMS_BLE_OBSERVER_PRIO
#define BLE_CPMS_BLE_OBSERVER_PRIO 2
#endif

// Supported Cycling Power features
#define BLE_CPMS_FEATURE_WHEEL_REVOLUTION_DATA_BIT (1 << 2)
#define BLE_CPMS_FEATURE_CRANK_REVOLUTION_DATA_BIT (1 << 3)

#define BLE_UUID_CYCLING_POWER_SERVICE			   0x1818
#define BLE_UUID_CYCLING_POWER_MEASUREMENT_CHAR	   0x2A63
#define BLE_UUID_CYCLING_POWER_VECTOR_CHAR		   0x2A64
#define BLE_UUID_CYCLING_POWER_FEATURE_CHAR		   0x2A65
#define BLE_UUID_CYCLING_POWER_CONTROL_POINT_CHAR  0x2A66

// maximum length of Cycling Power Measurement notification
#define BLE_CPMS_MAX_SC_MEAS_LEN 14

/**@brief   Macro for defining a ble_cpms_t instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CPMS_DEF(_name)  \
	static ble_cpms_t _name; \
	NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_CPMS_BLE_OBSERVER_PRIO, ble_cpms_on_ble_evt, &_name)

typedef struct
{
	uint16_t service_handle;
	ble_gatts_char_handles_t cycling_power_feature_handles;
	ble_gatts_char_handles_t cycling_power_measurement_handles;
	ble_gatts_char_handles_t sensor_location_handles;
	ble_gatts_char_handles_t cycling_power_control_point_handles;
	nrf_ble_gq_t *p_gatt_queue;
	bool cycling_power_measurement_notification_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	bool cycling_power_control_point_indication_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	uint32_t features;
} ble_cpms_t;

typedef struct
{
	security_req_t cycling_power_feature_read_access;
	security_req_t cycling_power_measurement_cccd_write_access;
	security_req_t cycling_power_sensor_location_read_access;

	security_req_t cycling_power_control_point_write_access;
	security_req_t cycling_power_control_point_cccd_access;

	ble_sensor_location_t sensor_location;
	nrf_ble_gq_t *p_gatt_queue;
	uint32_t features;
} ble_cpms_init_t;

typedef struct
{
	int16_t instantaneous_power_W;

	uint32_t total_wheel_revolutions;
	uint16_t last_wheel_event;
	bool speed_present;

	uint16_t total_crank_revolutions;
	uint16_t last_crank_event;
	bool cadence_present;
} ble_cpms_data_t;

/**@brief     Function for initializing the Cycling Power Service module.
 *
 * @param[in]   p_cpms       Pointer to the ble_cpms_t instance.
 * @param[in]   p_cpms_init  Pointer to the initialization structure.
 *
 * @retval     NRF_SUCCESS               If initialization was successful.
 * @retval     NRF_ERROR_INVALID_PARAM   If the init structure is invalid.
 */
ret_code_t ble_cpms_init(ble_cpms_t *const p_cpms, const ble_cpms_init_t *const p_cpms_init);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @param[in]   p_ble_evt  Bluetooth stack event.
 * @param[in]   p_context  Pointer to the ble_cpms_t instance.
 */
void ble_cpms_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief     Function for sending Cycling Power Measurement notification.
 *
 * @param[in]   p_cpms  Pointer to the ble_cpms_t instance.
 * @param[in]   p_data  Pointer to the measurement data.
 *
 * @retval     NRF_SUCCESS   If the notification was queued successfully.
 */
ret_code_t ble_cpms_update(ble_cpms_t *const p_cpms, const ble_cpms_data_t *const p_data);

#endif
