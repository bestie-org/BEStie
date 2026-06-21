/*! \file
	\ingroup group_app
	\brief Application specific settings
*/

#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

// <<< Use Configuration Wizard in Context Menu >>>\n

// <h> Application scheduler

/// <o> App_scheduler event size
//	<i> Default: 4
//	<i> Should be at least size of the biggest object passed to schedule handler
#define CFG_SCHED_EVT_SIZE 4

/// <o> Scheduled events queue size <3-20>
//	<i> Default: 10
//	<i> Timer and softdevice use 2 slots
#define CFG_SCHED_QUEUE_SIZE 10

// </h>

// <h> Bluetooth Low Energy

/// <o> Default connection tag <1-10>
//	<i> You shouldn't need to modify this
#define CFG_BLE_CONN_CFG_TAG 1

/// <o> Connection observer priority <1-10>
//	<i> you shouldn't need to change this
#define CFG_BLE_OBSERVER_PRIO 3

// <h> GAP settings

/// <s.20> Device Name
//	<i>	Used in Advertising packets and device information service
#define CFG_DEVICE_NAME "BEStie"

/// <o> Device Appearance
//	<i>	Used in Advertising packets
#define CFG_DEVICE_APPEARANCE BLE_APPEARANCE_CYCLING_CYCLING_COMPUTER

/// <o> SIG Assigned Manufacturer ID to be used in Manufacturer specific data <0x0001-0xFFFE>
#define CFG_COMPANY_IDENTIFIER 0xFFFE

/// <o> Minimum acceptable connection interval in ms <7-4000>
#define CFG_MIN_CONN_INTERVAL 100

/// <o> Maximum acceptable connection interval in ms <7-4000>
#define CFG_MAX_CONN_INTERVAL 250

/// <o> Slave latency in connection intervals <0-499>
#define CFG_SLAVE_LATENCY 3

/// <o> Connection supervisory timeout in ms <100-32000>
#define CFG_CONN_SUP_TIMEOUT 3000

/// <o> BLE link PHY
//	<i> Default: Auto
//
//	<0x00=> Automatically select highest supported PHY
//	<0x01=> 1Mbps PHY
//	<0x02=> 2Mbps PHY
#define CFG_BLE_PHY 0x01

// </h>

// <h> Connection parameters update module

/// <o> Time from initiating event (connect or start of notification) to first time
/// sd_ble_gap_conn_param_update is called in ms
//	<i> You may want to set this to higher value if connection process is slow
#define CFG_CONN_PARAM_FIRST_UPDATE_DELAY 5000

/// <o> Time between each call to sd_ble_gap_conn_param_update after the first call in ms
//	<i> Recommended default value is 30 seconds as per BLE spec
#define CFG_CONN_PARAM_NEXT_UPDATE_DELAY 30000

/// <o> Number of connection parameter update retries
#define CFG_CONN_PARAM_MAX_UPDATE_COUNT 3

/// <q> Should device disconnect if parameter update fails
#define CFG_CONN_PARAM_DISCONNECT_ON_FAIL 0

// </h>

// <h> Peer_manager settings

/// <q> Support bonding
#define CFG_SEC_PARAM_BOND 1

/// <q> Support man in the middle protection
#define CFG_SEC_PARAM_MITM 0

/// <q> Support LESC ECC based security
#define CFG_SEC_PARAM_LESC 1

// set LESC support flag in peer manager accordingly to save code space when LESC is not used
#define PM_LESC_ENABLED CFG_SEC_PARAM_LESC

/// <q> Support keypress notification
#define CFG_SEC_PARAM_KEYPRESS 0

/// <o> Supported IO capabilities for link authentication
//
//  <0x00=> BLE_GAP_IO_CAPS_DISPLAY_ONLY - Display Only
//  <0x01=> BLE_GAP_IO_CAPS_DISPLAY_YESNO - Display and Yes/No entry
//  <0x02=> BLE_GAP_IO_CAPS_KEYBOARD_ONLY - Keyboard Only
//  <0x03=> BLE_GAP_IO_CAPS_NONE - No I/O capabilities
//  <0x04=> BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY - Keyboard and Display
//
// <i> Default is no capabilities
#define CFG_SEC_PARAM_IO_CAPABILITIES 0x03

/// <q> Out of band key exchange support
#define CFG_SEC_PARAM_OOB 0

/// <o> Minimum key size <7-16>
//	<i> Default: 7
#define CFG_SEC_PARAM_MIN_KEY_SIZE 7

/// <o> Maximum key size <7-16>
//	<i> Must be larger than CFG_SEC_PARAM_MIN_KEY_SIZE
//	<i> Default: 16
#define CFG_SEC_PARAM_MAX_KEY_SIZE 16

/// <q> Automatically start connection security
#define CFG_PM_HANDLER_SECURE_ON_CONNECT 0

/// <q> Start connection security when access to protected service is attempted
#define CFG_PM_HANDLER_SECURE_ON_ERROR 0

// </h>

// <h> Queued Writes Manager
// <o> Maximum number of QWR enabled attributes <0-6>
#define CFG_QWR_MAX_ATTR 0

// propagate configuration to nrf_ble_qwr module
#define NRF_BLE_QWR_MAX_ATTR CFG_QWR_MAX_ATTR

// <o> Size of queued writes memory buffer in bytes <1-512>
#define CFG_QWR_MEM_SIZE 128
// </h>

// </h>

// <h> Misc application settings

// <h> eBike state handlers

/// <q> Enable Fitness Machine Service (indoor bike) support
//  <i> This works best but some legacy devices do not support it
//	<i> Default: 1
#ifndef CFG_FTMS_ENABLED
#define CFG_FTMS_ENABLED 1
#endif

/// <o> FTMS data update interval in ms <100->
//	<i> Default: 1000
#define CFG_FTMS_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS 1000

/// <q> Enable Cycling Power and Cycling speed and Cadence services
//  <i> This a backup solution intended for legacy devices
//	<i> Default: 0
#ifndef CFG_CPMS_CSC_ENABLED
#define CFG_CPMS_CSC_ENABLED 0
#endif

/// <o> CSC/CPMS data update interval in ms <100->
//	<i> Default: 1000
#define CSC_HANDLER_NOTIFICATION_TIMER_INTERVAL_MS 1000

#if !(defined(CFG_FTMS_ENABLED) && CFG_FTMS_ENABLED) && !(defined(CFG_CPMS_CSC_ENABLED) && CFG_CPMS_CSC_ENABLED)
#error "At least one data output service must be enabled. Check CFG_FTMS_ENABLED and CFG_CPMS_CSC_ENABLED"
#endif

// </h>
// </h>

#endif
