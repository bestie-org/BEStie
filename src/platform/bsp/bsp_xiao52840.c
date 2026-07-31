#if defined(BES_BSP_XIAO) && BES_BSP_XIAO

#include "nrf_delay.h"
#include "nrf_gpio.h"

#define __BSP_INTERNAL_H__
#include "bsp_xiao52840.h"

#define PIN_BATT_VOLTAGE_DIVIDER		  NRF_GPIO_PIN_MAP(0, 14)

#define PIN_BATT_VOLTAGE_DIVIDER_ACTIVE	  0 // active low
#define PIN_BATT_VOLTAGE_DIVIDER_INACTIVE 1

// pin that controls battery charging current
#define PIN_CHARGING_CURRENT NRF_GPIO_PIN_MAP(0, 13)

void bsp_board_init(void)
{
	// Xiao has ability to disable 1/2 battery voltage divider by setting P0.14 to low
	// this saves some power
	nrf_gpio_cfg_output(PIN_BATT_VOLTAGE_DIVIDER);
	nrf_gpio_pin_write(PIN_BATT_VOLTAGE_DIVIDER, PIN_BATT_VOLTAGE_DIVIDER_INACTIVE);

	// set slow charging, this is the safer option

	/**
	 * Xiao BLE wiki does not match schematics
	 * setting charging current pin P0.13 high as described here: https://wiki.seeedstudio.com/XIAO_BLE/#battery-charging-current
	 * causes charging process to never complete.
	 *
	 * Comparing resistance values with BQ25100 design reference (DS chapter 9.2.2.1.1 'Program the Fast Charge Current, ISET')
	 * using schematics and disregarding code on wiki:
	 * * P0.13 not connected to nRF crossbar == 2.7kOhm on ISET == 50mA current
	 * * P0.13 low == two 2.7kOhm resistors in parallel to ground == 100mA current
	 *
	 * this seems to align with behavior seen in the wild
	 */
	nrf_gpio_cfg_default(PIN_CHARGING_CURRENT); // this disconnects P0.13 from nRF GPIO crossbar regardless of previous state
}

void bsp_xiao_before_get_soc(void)
{
	nrf_gpio_pin_write(PIN_BATT_VOLTAGE_DIVIDER, PIN_BATT_VOLTAGE_DIVIDER_ACTIVE);

	// allow voltage to settle. 1ms should be plenty for 1M ohm / 1M ohm divider
	nrf_delay_ms(1);
}

void bsp_xiao_after_get_soc(void)
{
	nrf_gpio_pin_write(PIN_BATT_VOLTAGE_DIVIDER, PIN_BATT_VOLTAGE_DIVIDER_INACTIVE);
}

#endif // #if defined(BES_BSP_XIAO) && BES_BSP_XIAO
