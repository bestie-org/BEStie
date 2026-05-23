#include "advertising.h"
#include "app_scheduler.h"
#include "app_util_platform.h"
#include "ble_init.h"
#include "board_init.h"
#include "nrf_ble_lesc.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_pwr_mgmt.h"
#include "services.h"

static void idle_task(void)
{
	// execute app_scheduler tasks
	app_sched_execute();

#if CFG_SEC_PARAM_LESC

	// perform LESC key exchange if required
	ret_code_t err_code = nrf_ble_lesc_request_handler();
	if(err_code != NRF_ERROR_INVALID_STATE) {
		APP_ERROR_CHECK(err_code);
	}

#endif

	// process log events
	if(NRF_LOG_PROCESS() == false) {
		// low power sleep
		nrf_pwr_mgmt_run();
	}
}

int main(void)
{
	board_init();
	ble_init();
	services_init();
	advertising_init();
	APP_ERROR_CHECK(advertising_start());

	NRF_LOG_DEBUG("Program start");

	while(1) {
		idle_task();
	}
}
