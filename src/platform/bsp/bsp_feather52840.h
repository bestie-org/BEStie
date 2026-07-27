#ifndef __BSP_FEATHER52840_H__
#define __BSP_FEATHER52840_H__

// Adafruit Feather 52840 Express and Sense boards using single cell LiPo battery

#ifndef __BSP_INTERNAL_H__
#error "This file is meant to be included by bsp_internal.h"
#endif

#if !defined(BES_BSP_FEATHER) || !BES_BSP_FEATHER
#error "bsp_feather52840.h included but BES_BSP_FEATHER not defined"
#endif

#define BSP_BATT_SAADC_CHANNEL NRF_SAADC_INPUT_AIN5
#define BSP_BATT_VOLTAGE_MIN   3.0f
#define BSP_BATT_VOLTAGE_MAX   4.2f
#define BSP_BATT_VOLTAGE_SCALE 2.0f

#endif
