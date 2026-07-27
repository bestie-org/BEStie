#ifndef __BSP_H__
#define __BSP_H__

#include "nrfx_saadc.h"
#include "sdk_config.h"

// board variant specific procedures

// bsp setup that is ran once at power on
void bsp_init(void);

/**
 * Get battery state of charge as a value between 0.0f and 100.0f
 *
 * @param[in] p_result event handler for non blocking mode or NULL for blocking mode
 *
 * @retval NRF_ERROR_BUSY	another SAADC operation is in progress
 *
 * @retval NRF_SUCCESS		measurement will be performed
 */
ret_code_t bsp_get_soc(float *const p_result);

/**
 * Perform SAADC offset calibration
 * this should be done on boot and if temperature changes more than 10C
 *
 * Note: in blocking mode this takes 4ms and may interfere with radio operation
 *
 * @param[in] event_handler event handler for non blocking mode or NULL for blocking mode
 *
 * @retval NRF_ERROR_BUSY	another SAADC operation is in progress
 * @retval NRF_SUCCESS		calibration will be performed
 *
 */
ret_code_t bsp_saadc_calibrate(nrfx_saadc_event_handler_t event_handler);

#endif
