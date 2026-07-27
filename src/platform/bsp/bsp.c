#include "app_error.h"
#include "compiler_abstraction.h"
#include "nrfx_saadc.h"

#include "bsp.h"
#include "bsp_internal.h"

#define NRF_LOG_MODULE_NAME bsp
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static nrfx_saadc_channel_t saadc_battery_channel = NRFX_SAADC_DEFAULT_CHANNEL_SE(BSP_BATT_SAADC_CHANNEL, 0);

// stub to be overriden by defining this symbol elsewhere
__WEAK void bsp_board_init(void)
{
}

static void bsp_saadc_init(void)
{
	ret_code_t err_code;

	// allow adding board specific stuff before/after each measurement
	BSP_BATT_BEFORE_GET_SOC();

	err_code = nrfx_saadc_init(NRFX_SAADC_CONFIG_IRQ_PRIORITY);
	APP_ERROR_CHECK(err_code);

	// Xiao and Feather have 1M/1M divider = ~500k source impedance, default 10us TACQ is too short
	saadc_battery_channel.channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

	err_code = nrfx_saadc_channels_config(&saadc_battery_channel, 1);
	APP_ERROR_CHECK(err_code);

	// null handler == blocking mode
	err_code = bsp_saadc_calibrate(NULL);
	APP_ERROR_CHECK(err_code);

	// allow adding board specific stuff before/after each measurement
	BSP_BATT_AFTER_GET_SOC();
}

static inline uint16_t nrf_saadc_resolution_to_steps(const nrf_saadc_resolution_t resolution)
{
	switch(resolution) {
	case NRF_SAADC_RESOLUTION_8BIT:
		return 256;
	case NRF_SAADC_RESOLUTION_10BIT:
		return 1024;
	case NRF_SAADC_RESOLUTION_12BIT:
		return 4096;
	case NRF_SAADC_RESOLUTION_14BIT:
		return 16384;
	default:
		APP_ERROR_CHECK(NRF_ERROR_INVALID_DATA);
		return 1;
	}
}

static inline float saadc_value_to_soc(const nrf_saadc_value_t *const p_saadc_value, const nrf_saadc_resolution_t resolution)
{
	ASSERT(p_saadc_value);

	// 0.6V internal reference, 1/6 prescaler
	float saadc_val_V = 3.6f * BSP_BATT_VOLTAGE_SCALE * (*p_saadc_value) / nrf_saadc_resolution_to_steps(resolution);

	// simple linear mapping, good enough for a device that draws less than 0.2mA
	float soc =
		(saadc_val_V + BSP_BATT_VOLTAGE_OFFSET - BSP_BATT_VOLTAGE_MIN) / (BSP_BATT_VOLTAGE_MAX - BSP_BATT_VOLTAGE_MIN) * 100.0f;

	NRF_LOG_DEBUG("Raw ADC: %d counts, Raw Voltage " NRF_LOG_FLOAT_MARKER "V", *p_saadc_value, NRF_LOG_FLOAT(saadc_val_V));
	NRF_LOG_DEBUG("Corrected Voltage: " NRF_LOG_FLOAT_MARKER "V, " NRF_LOG_FLOAT_MARKER "V offset",
				  NRF_LOG_FLOAT((saadc_val_V + BSP_BATT_VOLTAGE_OFFSET)), NRF_LOG_FLOAT(BSP_BATT_VOLTAGE_OFFSET));
	NRF_LOG_DEBUG("Raw SOC: " NRF_LOG_FLOAT_MARKER "%%", NRF_LOG_FLOAT(soc));

	if(soc < 0.0f) {
		soc = 0.0f;
	}

	if(soc > 100.0f) {
		soc = 100.0f;
	}

	return soc;
}

ret_code_t bsp_get_soc(float *const p_result)
{
	ASSERT(p_result);

	ret_code_t err_code;
	nrf_saadc_value_t saadc_value = {0};

	// allow adding board specific stuff before/after each measurement
	BSP_BATT_BEFORE_GET_SOC();

	// null handler == blocking mode
	err_code = nrfx_saadc_simple_mode_set(1 << 0, NRFX_SAADC_CONFIG_RESOLUTION, NRFX_SAADC_CONFIG_OVERSAMPLE, NULL);
	VERIFY_SUCCESS(err_code);

	err_code = nrfx_saadc_buffer_set(&saadc_value, 1);
	VERIFY_SUCCESS(err_code);

	err_code = nrfx_saadc_mode_trigger();
	VERIFY_SUCCESS(err_code);

	// disconnect ADC from GPIO to protect it in case direct battery voltage appears on Xiao boards
	nrf_saadc_channel_input_set(0, NRF_SAADC_INPUT_DISABLED, NRF_SAADC_INPUT_DISABLED);

	// allow adding board specific stuff before/after each measurement
	BSP_BATT_AFTER_GET_SOC();

	*p_result = saadc_value_to_soc(&saadc_value, NRFX_SAADC_CONFIG_RESOLUTION);

	return NRF_SUCCESS;
}

ret_code_t bsp_saadc_calibrate(nrfx_saadc_event_handler_t event_handler)
{
	return nrfx_saadc_offset_calibrate(event_handler);
}

void bsp_init(void)
{
	bsp_board_init();
	bsp_saadc_init();
}
