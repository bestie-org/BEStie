#ifndef __BSP_INTERNAL_H__
#define __BSP_INTERNAL_H__

// including nrf_gpio.h for respecitve macro results in circular dependency
#define BSP_GPIO_PIN_MAP(port, pin) (((port) << 5) | ((pin) & 0x1F))

// include selected board variant header
#if defined(BES_BSP_GENERIC) && BES_BSP_GENERIC
#include "bsp_generic.h"
#elif defined(BES_BSP_FEATHER) && BES_BSP_FEATHER
#include "bsp_feather52840.h"
#elif defined(BES_BSP_XIAO) && BES_BSP_XIAO
#include "bsp_xiao52840.h"
#else
#error "Board variant undefined"
#endif

#ifndef BSP_BATT_VOLTAGE_SCALE
#define BSP_BATT_VOLTAGE_SCALE 1.0f
#endif

#ifndef BSP_BATT_VOLTAGE_OFFSET
#define BSP_BATT_VOLTAGE_OFFSET 0.0f
#endif

// dummy hook macros
#ifndef BSP_BATT_BEFORE_GET_SOC
#define BSP_BATT_BEFORE_GET_SOC()
#endif

#ifndef BSP_BATT_AFTER_GET_SOC
#define BSP_BATT_AFTER_GET_SOC()
#endif

#endif
