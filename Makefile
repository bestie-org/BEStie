# nRF5x SDK installation directory
NRF_SDK_ROOT:= ./external/nRF5_SDK

# project directory relative to Makefile
PROJ_DIR:=.

OUTPUT_DIRECTORY ?= build

# list of enabled Nordic nRF5 SDK modules
# module name can match directory inside components/ or external/ or .c file in nrfx legacy and driver source tree
NRF_SDK_MODULES:=common util delay segger_rtt log log/src memobj strerror fprintf experimental_section_vars balloc ringbuf atomic pwr_mgmt mutex \
					scheduler timer nrf_ble_qwr nrf_ble_gatt atomic_flags peer_manager fds atomic_fifo fstorage stack_info \
					queue sortlist timer/experimental nrf_oberon mpu stack_guard fprintf crypto crypto/backend crypto/backend/oberon \
					crypto/backend/nrf_hw nrf_drv_rng nrfx_rng hardfault hardfault/nrf52/handler ble_db_discovery nrf_ble_gq

# list of modules to be used only as source of header files
NRF_SDK_INCLUDE_ONLY_MODULES:=crypto/backend/micro_ecc crypto/backend/cc310 crypto/backend/cifra crypto/backend/nrf_sw \
								crypto/backend/cc310_bl crypto/backend/mbedtls crypto/backend/optiga nrf_oberon/include nrf_cc310/include

BES_RAM_ORIGIN_NRF52832 := 0x20004A08
BES_RAM_LENGTH_NRF52832 := 0xB5F8

BES_RAM_ORIGIN_NRF52840 := 0x20004A18
BES_RAM_LENGTH_NRF52840 := 0x3B5E8

include protobuf.mk
include nrf5_sdk.mk
