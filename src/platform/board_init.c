#include "board_init.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_mpu_lib.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_stack_guard.h"

static void init_log(void)
{
	ret_code_t err_code = NRF_LOG_INIT(NULL);
	APP_ERROR_CHECK(err_code);

	NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void init_scheduler(void)
{
	APP_SCHED_INIT(MAX(CFG_SCHED_EVT_SIZE, sizeof(app_timer_event_t)), CFG_SCHED_QUEUE_SIZE);
}

static void init_timer(void)
{
	ret_code_t err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);
}

static void init_pwr_mgmt(void)
{
	ret_code_t err_code;
	err_code = nrf_pwr_mgmt_init();
	APP_ERROR_CHECK(err_code);
}

static void init_stack_guard(void)
{
	ret_code_t err_code = nrf_mpu_lib_init();
	APP_ERROR_CHECK(err_code);

	err_code = nrf_stack_guard_init();
	APP_ERROR_CHECK(err_code);
}

void board_init(void)
{
	init_log();
	init_scheduler();
	init_timer();
	init_pwr_mgmt();
	init_stack_guard();
}
