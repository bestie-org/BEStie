#ifndef __BLE_LDI_C_H__
#define __BLE_LDI_C_H__

#include "ble.h"
#include "ble_db_discovery.h"
#include "ble_srv_common.h"
#include "ebike_live_data.pb.h"
#include "nrf_ble_gq.h"
#include "nrf_sdh_ble.h"
#include <stdint.h>

#ifndef BLE_LDI_C_BLE_OBSERVER_PRIO
#define BLE_LDI_C_BLE_OBSERVER_PRIO 2
#endif

#define BLE_UUID_LDI_SERVICE 0xEB20
#define BLE_UUID_LDI_CHAR	 0xEB21

/**@brief   Macro for defining a ble_ldi_c instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_LDI_C_DEF(_name)  \
	static ble_ldi_c_t _name; \
	NRF_SDH_BLE_OBSERVER(_name##_obs, BLE_LDI_C_BLE_OBSERVER_PRIO, ble_ldi_c_on_ble_evt, &_name)

/**@brief   Macro for defining multiple ble_ldi_c instances.
 *
 * @param   _name   Name of the array of instances.
 * @param   _cnt    Number of instances to define.
 */
#define BLE_LDI_C_ARRAY_DEF(_name, _cnt) \
	static ble_ldi_c_t _name[_cnt];      \
	NRF_SDH_BLE_OBSERVERS(_name##_obs, BLE_LDI_C_BLE_OBSERVER_PRIO, ble_ldi_c_on_ble_evt, &_name, _cnt)

typedef com_bosch_ebike_LiveData ble_ldi_t;

typedef enum
{
	BLE_LDI_C_EVT_DISCOVERY_COMPLETE = 1, // peer's GATT sever has Live Data Interface service
	BLE_LDI_C_EVT_LIVE_DATA_NOTIFICATION, // incoming Live Data Interface Characteristic notification
	BLE_LDI_C_EVT_LIVE_DATA_READ,		  // Live Data Interface Characteristic read response as a result of ble_ldi_c_ldi_read()
	BLE_LDI_C_EVT_DISCONNECT,			  // peer that hosts Live Data Interface service has disconnected
} ble_ldi_c_evt_type_t;

typedef struct
{
	uint16_t live_data_cccd_handle;
	uint16_t live_data_char_handle;
} ldi_db_t;

typedef struct
{
	ble_ldi_c_evt_type_t evt_type;
	uint16_t conn_handle;
	union
	{
		ldi_db_t peer_db; /* LDI related handles found on peer device. This is filled if the evt_type is
							 BLE_LDI_C_EVT_DISCOVERY_COMPLETE. */
		ble_ldi_t ldi;	  /* Live eBike Data. This is filled if the evt_type is BLE_LDI_C_EVT_LIVE_DATA_NOTIFICATION and
							 BLE_LDI_C_EVT_LIVE_DATA_READ */
	} params;
} ble_ldi_c_evt_t;

typedef struct ble_ldi_c_s ble_ldi_c_t;
typedef void (*ble_ldi_c_evt_handler_t)(ble_ldi_c_t *p_ble_ldi_c, ble_ldi_c_evt_t *p_evt);

struct ble_ldi_c_s
{
	uint16_t conn_handle;				   /* Connection handle, as provided by the SoftDevice. */
	ldi_db_t peer_ldi_db;				   /* Handles related to LDI on the peer. */
	ble_ldi_c_evt_handler_t evt_handler;   /* Application event handler to be called when there is an event related to the Live
											  eBike Data Interface Service. */
	ble_srv_error_handler_t error_handler; /* Function to be called in case of an error. */
	nrf_ble_gq_t *p_gatt_queue;			   /* Pointer to the BLE GATT Queue instance. */
};

typedef struct
{
	ble_ldi_c_evt_handler_t evt_handler;   /* Event handler to be called by the Live Data Interface Client module when there is an
											  event related to the Live Data Interface Service. */
	ble_srv_error_handler_t error_handler; /* Function to be called in case of an error. */
	nrf_ble_gq_t *p_gatt_queue;			   /* Pointer to the BLE GATT Queue instance. */
} ble_ldi_c_init_t;

uint8_t ble_ldi_c_get_base_type(void);

/**@brief     Function for handling BLE events from the SoftDevice.
 *
 * @details   This function handles the BLE events received from the SoftDevice. If a BLE
 *            event is relevant to the Live Data Interface Client module, the function uses the event's data to update
 *            interval variables and, if necessary, send events to the application.
 *
 * @param[in] p_ble_evt     Pointer to the BLE event.
 * @param[in] p_context     Pointer to the Heart Rate Client structure.
 */
void ble_ldi_c_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief     Function for initializing the Live Data Interface Client module.
 *
 * @details   This function registers with the Database Discovery module for the Live Data Interface Service.
 *		   	  The module looks for the presence of a Live Data Interface Service instance at the peer
 *            when a discovery is started.
 *
 * @param[in] p_ble_ldi_c      Pointer to the Live Data Interface Client structure.
 * @param[in] p_ble_ldi_c_init Pointer to the Live Data Interface initialization structure that contains
 *                             the initialization information.
 *
 * @retval    NRF_SUCCESS On successful initialization.
 * @retval    err_code    Otherwise, this function propagates the error code returned by the Database Discovery module API
 *                        @ref ble_db_discovery_evt_register.
 */
ret_code_t ble_ldi_c_init(ble_ldi_c_t *const p_ble_ldi_c, ble_ldi_c_init_t *const p_ble_ldi_c_init);

/**@brief   Function for requesting the peer to start sending notification of Live Data Interface updates.
 *
 * @details This function enables notification of the Live Data Interface updates at the peer
 *          by writing to the CCCD of the Live Data Interface characteristic.
 *
 * @param   p_ble_ldi_c Pointer to the Live Data Interface Client structure.
 *
 * @retval  NRF_SUCCESS If the SoftDevice is requested to write to the CCCD of the peer.
 * @retval	err_code	Otherwise, this function propagates the error code returned
 *                      by the SoftDevice API @ref sd_ble_gattc_write.
 */
ret_code_t ble_ldi_c_ldi_notif_enable(ble_ldi_c_t *const p_ble_ldi_c);

/**@brief   Function for requesting readout of initial state of Live Data Interface characteristic.
 *
 * @details This function schedules data read operation of Live Data Interface characteristic. Results are delivered in
 * BLE_LDI_C_EVT_LIVE_DATA_READ event.
 *
 * @param   p_ble_ldi_c Pointer to the Live Data Interface Client structure.
 *
 * @retval  NRF_SUCCESS If the operation is put in GATT queue succesfully.
 * @retval	err_code	Otherwise, this function propagates the error code returned
 *                      by the GATT queue API.
 */
ret_code_t ble_ldi_c_ldi_read(ble_ldi_c_t *const p_ble_ldi_c);

/**@brief     Function for handling events from the Database Discovery module.
 *
 * @details   Call this function when you get a callback event from the Database Discovery module.
 *            This function handles an event from the Database Discovery module and determines
 *            whether it relates to the discovery of Live Data Interface Service at the peer. If it does, the function
 *            calls the application's event handler to indicate that the Live Data Interface Service was
 *            discovered at the peer. The function also populates the event with service-related
 *            information before providing it to the application.
 *
 * @param[in] p_ble_ldi_c Pointer to the Live Data Interface Client structure instance for associating the link.
 * @param[in] p_evt Pointer to the event received from the Database Discovery module.
 *
 */
void ble_ldi_c_on_db_disc_evt(ble_ldi_c_t *p_ble_ldi_c, const ble_db_discovery_evt_t *p_evt);

/**@brief     Function for assigning handles to an instance of ldi_c.
 *
 * @details   Call this function when a link has been established with a peer to
 *            associate the link to this instance of the module. This association makes it
 *            possible to handle several links and associate each link to a particular
 *            instance of this module. The connection handle and attribute handles are
 *            provided from the discovery event @ref BLE_LDI_C_EVT_DISCOVERY_COMPLETE.
 *
 * @param[in] p_ble_ldi_c        Pointer to the Live Data Interface Client structure instance for associating the link.
 * @param[in] conn_handle        Connection handle to associate with the given Live Data Interface Client Instance.
 * @param[in] p_peer_ldi_handles Attribute handles for the LDI server you want this LDI_C client to
 *                               interact with.
 */
ret_code_t ble_ldi_c_handles_assign(ble_ldi_c_t *p_ble_ldi_c, uint16_t conn_handle, const ldi_db_t *p_peer_ldi_handles);

#endif
