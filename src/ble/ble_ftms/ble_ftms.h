#ifndef __BLE_FTMS_H__
#define __BLE_FTMS_H__

#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_ble_gq.h"
#include "nrf_sdh_ble.h"
#include "sdk_config.h"

#ifndef BLE_FTMS_BLE_OBSERVER_PRIO
#define BLE_FTMS_BLE_OBSERVER_PRIO 2
#endif

// maximum length of Indoor Bike Data notification
#define BLE_FTMS_MAX_INDOOR_BIKE_DATA_LEN 18

/**@brief   Macro for defining a ble_ftms_t instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_FTMS_DEF(_name)  \
	static ble_ftms_t _name; \
	NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_FTMS_BLE_OBSERVER_PRIO, ble_ftms_on_ble_evt, &_name)

// BLE Fitness Machine service (16 bit UUIDs)
#define BLE_UUID_FTMS_SERVICE				0x1826
#define BLE_UUID_FTMS_FEATURE_CHAR			0x2ACC
#define BLE_UUID_FTMS_INDOOR_BIKE_DATA_CHAR 0x2AD2
#define BLE_UUID_FTMS_CONTROL_POINT_CHAR	0x2AD9
#define BLE_UUID_FTMS_STATUS_CHAR			0x2ADA // mandatory if control point is supported

typedef struct
{
	uint16_t service_handle;
	ble_gatts_char_handles_t ftms_feature_handles;
	ble_gatts_char_handles_t indoor_bike_data_handles;
	ble_gatts_char_handles_t ftms_control_point_handles;
	ble_gatts_char_handles_t ftms_status_handles;

	nrf_ble_gq_t *p_gatt_queue;
	bool indoor_bike_data_notification_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	bool ftms_control_indication_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
	bool ftms_status_notification_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
} ble_ftms_t;

typedef struct
{
	security_req_t ftms_feature_read_access;

	security_req_t indoor_bike_data_cccd_access;

	security_req_t ftms_control_point_write_access;
	security_req_t ftms_control_point_cccd_access;

	security_req_t ftms_status_cccd_access;

	nrf_ble_gq_t *p_gatt_queue;
} ble_ftms_init_t;

typedef struct
{
	float instantaneous_speed_kph;
	bool instantaneous_speed_present;
	uint16_t instantaneous_cadence_rpm;
	bool instantaneous_cadence_present;
	uint32_t total_distance_m; // catch: uint24!!
	bool total_distance_present;
	int16_t instantaneous_power_W;
	bool instantaneous_power_present;
	int16_t average_cadence_rpm;
	bool average_cadence_present;
	int16_t average_power_W;
	bool average_power_present;

} ble_ftms_data_t;

typedef enum
{
	BLE_FTMS_STATUS_PAUSED,
	BLE_FTMS_STATUS_RESUMED,
} ble_ftms_status_t;

/**@brief     Function for initializing the Fitness Machine Service module.
 *
 * @param[in]   p_ftms      Pointer to the ble_ftms_t instance.
 * @param[in]   p_ftms_init Pointer to the initialization structure.
 *
 * @retval     NRF_SUCCESS               If initialization was successful.
 * @retval     NRF_ERROR_INVALID_PARAM   If the init structure is invalid.
 */
ret_code_t ble_ftms_init(ble_ftms_t *const p_ftms, const ble_ftms_init_t *const p_ftms_init);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @param[in]   p_ble_evt  Bluetooth stack event.
 * @param[in]   p_context  Pointer to the ble_ftms_t instance.
 */
void ble_ftms_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief     Function for sending Indoor Bike Data notification.
 *
 * @param[in]   p_ftms  Pointer to the ble_ftms_t instance.
 * @param[in]   p_data  Pointer to the Indoor Bike Data measurement.
 *
 * @retval     NRF_SUCCESS   If the notification was queued successfully.
 */
ret_code_t ble_ftms_measurement_update(ble_ftms_t *const p_ftms, const ble_ftms_data_t *const p_data);

/**@brief     Function for sending Fitness Machine Status notification.
 *
 * @param[in]   p_ftms  Pointer to the ble_ftms_t instance.
 * @param[in]   status  Status to send.
 *
 * @retval     NRF_SUCCESS               If the notification was queued successfully.
 * @retval     NRF_ERROR_INVALID_PARAM   If the status is invalid.
 */
ret_code_t ble_ftms_status_send(ble_ftms_t *const p_ftms, ble_ftms_status_t status);

#endif
