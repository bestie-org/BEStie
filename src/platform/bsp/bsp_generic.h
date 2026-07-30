#ifndef __BSP_GENERIC_H__
#define __BSP_GENERIC_H__

// generic board variant that is powered by coin cell battery measured directly by VDD pin

#ifndef __BSP_INTERNAL_H__
#error "This file is meant to be included by bsp_internal.h"
#endif

#if !defined(BES_BSP_GENERIC) || !BES_BSP_GENERIC
#error "bsp_generic.h included but BES_BSP_GENERIC not defined"
#endif

#define BSP_BATT_SAADC_CHANNEL NRF_SAADC_INPUT_VDD
#define BSP_BATT_VOLTAGE_MIN   1.8f
#define BSP_BATT_VOLTAGE_MAX   3.0f
#define BSP_BATT_VOLTAGE_SCALE 1.0f

// typical protection diode voltage drop for 1mA current draw
#define BSP_BATT_VOLTAGE_OFFSET 0.270f

#if defined(BES_BOARD_NRF52_DK) && BES_BOARD_NRF52_DK

#define BSP_UART_TX BSP_GPIO_PIN_MAP(0,6)
#define BSP_UART_RX BSP_GPIO_PIN_MAP(0,8)

#elif defined(BES_BOARD_NRF52840_DK) && BES_BOARD_NRF52840_DK

#define BSP_UART_TX BSP_GPIO_PIN_MAP(0,6)
#define BSP_UART_RX BSP_GPIO_PIN_MAP(0,8)

#else
#error "Neither nRF52-DK or nRF52840-DK is selected"
#endif

#endif
