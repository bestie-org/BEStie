#if defined(BES_BSP_XIAO) && BES_BSP_XIAO

#include "nrf_delay.h"
#include "nrf_gpio.h"

#define __BSP_INTERNAL_H__
#include "bsp_xiao52840.h"

#define PIN_BATT_VOLTAGE_DIVIDER		  NRF_GPIO_PIN_MAP(0, 14)

#define PIN_BATT_VOLTAGE_DIVIDER_ACTIVE	  0 // active low
#define PIN_BATT_VOLTAGE_DIVIDER_INACTIVE 1

// pin that controls battery charging current
#define PIN_CHARGING_CURRENT	  NRF_GPIO_PIN_MAP(0, 13)
#define PIN_CHARGING_CURRENT_SLOW 1 // 50mA current
#define PIN_CHARGING_CURRENT_FAST 0 // 100mA current

void bsp_board_init(void)
{
	// Xiao has ability to disable 1/2 battery voltage divider by setting P0.14 to low
	// this saves some power
	nrf_gpio_cfg_output(PIN_BATT_VOLTAGE_DIVIDER);
	nrf_gpio_pin_write(PIN_BATT_VOLTAGE_DIVIDER, PIN_BATT_VOLTAGE_DIVIDER_INACTIVE);

	// set slow charging, this is the safer option
	nrf_gpio_cfg_output(PIN_CHARGING_CURRENT);
	nrf_gpio_pin_write(PIN_CHARGING_CURRENT, PIN_CHARGING_CURRENT_SLOW);
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
