#ifndef __BSP_XIAO52840_H__
#define __BSP_XIAO52840_H__

// Seed Studio Xiao BLE 'basic' and Sense boards using single cell LiPo battery

#ifndef __BSP_INTERNAL_H__
#error "This file is meant to be included by bsp_internal.h"
#endif

#if !defined(BES_BSP_XIAO) || !BES_BSP_XIAO
#error "bsp_xiao52840.h included but BES_BSP_XIAO not defined"
#endif

#define BSP_BATT_SAADC_CHANNEL NRF_SAADC_INPUT_AIN7
#define BSP_BATT_VOLTAGE_MIN   3.0f
#define BSP_BATT_VOLTAGE_MAX   4.10f

// 510K/1M divider according to the schematics
#define BSP_BATT_VOLTAGE_SCALE (1510000.0f/510000.0f)

// drive 1/2 VDD prescaler on AIN7
#define BSP_BATT_BEFORE_GET_SOC() bsp_xiao_before_get_soc()
#define BSP_BATT_AFTER_GET_SOC()  bsp_xiao_after_get_soc()

#define BSP_UART_TX BSP_GPIO_PIN_MAP(1,11)
#define BSP_UART_RX BSP_GPIO_PIN_MAP(1,12)

void bsp_xiao_before_get_soc(void);
void bsp_xiao_after_get_soc(void);

#endif
