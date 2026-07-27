#ifndef _BLE_BAS_H__
#define _BLE_BAS_H__

/**
 * Multiperipheral capable version of BLE Battery Service
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

#ifndef BLE_BAS_BLE_OBSERVER_PRIO
#define BLE_BAS_BLE_OBSERVER_PRIO 2
#endif

#define BLE_BAS_MAX_BATTERY_LEVEL_DATA_LEN sizeof(uint8_t)

/**@brief   Macro for defining a ble_bas_t instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_BAS_DEF(_name)  \
	static ble_bas_t _name; \
	NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_BAS_BLE_OBSERVER_PRIO, ble_bas_on_ble_evt, &_name)

typedef struct
{
	uint16_t service_handle;
	ble_gatts_char_handles_t battery_level_handles;
	nrf_ble_gq_t *p_gatt_queue;
	bool battery_level_notification_enabled[NRF_SDH_BLE_PERIPHERAL_LINK_COUNT];
} ble_bas_t;

typedef struct
{
	security_req_t battery_level_cccd_access;
	security_req_t battery_level_read_access;

	nrf_ble_gq_t *p_gatt_queue;
} ble_bas_init_t;

/**@brief     Function for initializing the Battery Service module.
 *
 * @param[in]   p_bas       Pointer to the ble_bas_t instance.
 * @param[in]   p_bas_init  Pointer to the initialization structure.
 *
 * @retval     NRF_SUCCESS               If initialization was successful.
 * @retval     NRF_ERROR_INVALID_PARAM   If the init structure is invalid.
 */
ret_code_t ble_bas_init(ble_bas_t *const p_bas, const ble_bas_init_t *const p_bas_init);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @param[in]   p_ble_evt  Bluetooth stack event.
 * @param[in]   p_context  Pointer to the ble_bas_t instance.
 */
void ble_bas_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief     Function for sending Battery Level notification.
 *
 * @param[in]   p_bas   Pointer to the ble_bas_t instance.
 * @param[in]   battery_level_pct  Batterly level in percent
 *
 * @retval     NRF_SUCCESS   If the notification was queued successfully.
 */
ret_code_t ble_bas_update(ble_bas_t *const p_bas, const uint8_t battery_level_pct);

#endif
