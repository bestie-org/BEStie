/**
 * BLE initalization routines
 *
 * */

#ifndef __BLE_INIT_H__
#define __BLE_INIT_H__

#include "nrf_ble_gatt.h"

// initalize BLE platform
void ble_init(void);

// return handle to nRF BLE GATT module
nrf_ble_gatt_t *ble_nrf_gatt_get(void);

#endif
