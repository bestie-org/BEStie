#ifndef __SERVICES_H__
#define __SERVICES_H__

#include "ble_cpms.h"
#include "ble_csc.h"
#include "ble_ftms.h"

// initialize BLE services module
void services_init(void);

// get service instance handles
ble_cpms_t *services_cpms_inst_get(void);
ble_csc_t *services_csc_inst_get(void);
ble_ftms_t *services_ftms_inst_get(void);

#endif
