# ================================================
#
#   nRF5 SDK convinience wrapper
#
# ================================================

TARGETS          := nrf52832_xxaa nrf52840_xxaa
OUTPUT_DIRECTORY ?= build

# build and board selection handling

# list of supported values
SUPPORTED_BUILDS := debug release size size_debug
SUPPORTED_BOARDS := nrf52832dk nrf52840dk

# defaults
DEFAULT_BUILD := debug
DEFAULT_BOARD := nrf52832dk
USE_NFC_AS_GPIO ?= 0

# if variable is not specified use defaults

BUILD ?= $(DEFAULT_BUILD)
BOARD ?= $(DEFAULT_BOARD)

# error if variable is set to unsupported value

ifeq ($(filter $(BUILD),$(SUPPORTED_BUILDS)),)
$(error BUILD=$(BUILD) is not supported. Run 'make help' for help)
endif

ifeq ($(filter $(BOARD),$(SUPPORTED_BOARDS)),)
$(error BOARD=$(BOARD) is not supported. Run 'make help' for help)
endif

# apply build specific flags

# debug mode settings
ifeq ($(BUILD),debug)
	COMMON_FLAGS += -DDEBUG=1 -DDEBUG_NRF_USER=1
	CFLAGS += -Og

# release mode settings
else ifeq ($(BUILD),release)
	COMMON_FLAGS += -DRELEASE=1 -DNRF_LOG_DEFAULT_LEVEL=3
	CFLAGS += -O3

# size minimalization mode settings
else ifeq ($(BUILD),size)
	COMMON_FLAGS += -DRELEASE=1 -DNRF_LOG_DEFAULT_LEVEL=3
	CFLAGS += -Os

else ifeq ($(BUILD),size_debug)
	COMMON_FLAGS += -DDEBUG=1 -DDEBUG_NRF_USER=1
	CFLAGS += -Os

# unknown mode
else
	$(error Error setting BUILD=$(BUILD) dependent flags, this should never happen!)
endif

# apply board specific settings

# nrf52832dk
ifeq ($(BOARD),nrf52832dk)
	COMMON_FLAGS += -DBES_BOARD_NRF52_DK=1

	# set MCU
	.DEFAULT_GOAL := nrf52832_xxaa

# nrf52840dk
else ifeq ($(BOARD),nrf52840dk)
	COMMON_FLAGS += -DBES_BOARD_NRF52840_DK=1

	# set MCU
	.DEFAULT_GOAL := nrf52840_xxaa

# unknown board
else
	$(error Error setting BOARD=$(BOARD) dependent flags, this should never happen!)
endif

# nRF52832 specific settings
ifeq ($(.DEFAULT_GOAL),nrf52832_xxaa)
	LIB_FILES += $(NRF_SDK_ROOT)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_3.0.8.a
	SRC_FILES  += $(NRF_SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c $(NRF_SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52.S

	COMMON_FLAGS += -DFLOAT_ABI_HARD
	COMMON_FLAGS += -DNRF52
	COMMON_FLAGS += -DNRF52832_XXAA
	COMMON_FLAGS += -DNRF52_PAN_74
	COMMON_FLAGS += -DS132
	COMMON_FLAGS += -mcpu=cortex-m4
	COMMON_FLAGS += -mthumb -mabi=aapcs
	COMMON_FLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

	LDFLAGS += -mthumb -mabi=aapcs
	LDFLAGS += -mcpu=cortex-m4
	LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

	# add S132 softdevice as compilation target
	NRF_SDK_DIRS += $(NRF_SDK_ROOT)/components/softdevice/s132/headers $(NRF_SDK_ROOT)/components/softdevice/s132/headers/nrf52

	# softdevice .hex file
	BES_SOFTDEVICE_HEX ?= $(NRF_SDK_ROOT)/components/softdevice/s132/hex/s132_nrf52_7.3.0_softdevice.hex

	# linker script
	LINKER_SCRIPT:= $(PROJ_DIR)/nrf52832_template.ld

	# pass RAM and FLASH origin and size to linker script

	# default values, update this each time softdevice version changes
	BES_FLASH_ORIGIN_NRF52832 ?= 0x26000
	BES_FLASH_LENGTH_NRF52832 ?= 0x5a000
	BES_RAM_ORIGIN_NRF52832 ?= 0x20002260
	BES_RAM_LENGTH_NRF52832 ?= 0xDDA0

	LDFLAGS += -Wl,--defsym=BES_FLASH_ORIGIN_NRF52832=$(BES_FLASH_ORIGIN_NRF52832)
	LDFLAGS += -Wl,--defsym=BES_FLASH_LENGTH_NRF52832=$(BES_FLASH_LENGTH_NRF52832)
	LDFLAGS += -Wl,--defsym=BES_RAM_ORIGIN_NRF52832=$(BES_RAM_ORIGIN_NRF52832)
	LDFLAGS += -Wl,--defsym=BES_RAM_LENGTH_NRF52832=$(BES_RAM_LENGTH_NRF52832)

# nRF52840 specific settings
else ifeq ($(.DEFAULT_GOAL),nrf52840_xxaa)
	LIB_FILES += $(NRF_SDK_ROOT)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_3.0.8.a
	SRC_FILES  += $(NRF_SDK_ROOT)/modules/nrfx/mdk/system_nrf52840.c $(NRF_SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S

	COMMON_FLAGS += -DFLOAT_ABI_HARD
	COMMON_FLAGS += -DNRF52840_XXAA
	COMMON_FLAGS += -DS140
	COMMON_FLAGS += -mcpu=cortex-m4
	COMMON_FLAGS += -mthumb -mabi=aapcs
	COMMON_FLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

	LDFLAGS += -mthumb -mabi=aapcs
	LDFLAGS += -mcpu=cortex-m4
	LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

	# add S140 softdevice as compilation target
	NRF_SDK_DIRS += $(NRF_SDK_ROOT)/components/softdevice/s140/headers $(NRF_SDK_ROOT)/components/softdevice/s140/headers/nrf52

	# softdevice .hex file
	BES_SOFTDEVICE_HEX ?= $(NRF_SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.3.0_softdevice.hex

	# linker script
	LINKER_SCRIPT:= $(PROJ_DIR)/nrf52840_template.ld

	# pass RAM and FLASH origin and size to linker script

	BES_FLASH_ORIGIN_NRF52840 ?= 0x27000
	BES_FLASH_LENGTH_NRF52840 ?= 0xd9000
	BES_RAM_ORIGIN_NRF52840 ?= 0x20002270
	BES_RAM_LENGTH_NRF52840 ?= 0x3DD90

	LDFLAGS += -Wl,--defsym=BES_FLASH_ORIGIN_NRF52840=$(BES_FLASH_ORIGIN_NRF52840)
	LDFLAGS += -Wl,--defsym=BES_FLASH_LENGTH_NRF52840=$(BES_FLASH_LENGTH_NRF52840)
	LDFLAGS += -Wl,--defsym=BES_RAM_ORIGIN_NRF52840=$(BES_RAM_ORIGIN_NRF52840)
	LDFLAGS += -Wl,--defsym=BES_RAM_LENGTH_NRF52840=$(BES_RAM_LENGTH_NRF52840)

# unknown architecture
else
	$(error MCU arch not defined, this should never happen!)
endif

# common Makefile definitions

BES_STACK_SIZE ?= 8192

# Flags common for C and assembler
COMMON_FLAGS += -DBLE_STACK_SUPPORT_REQD
COMMON_FLAGS += -DAPP_TIMER_V2
COMMON_FLAGS += -DAPP_TIMER_V2_RTC1_ENABLED
COMMON_FLAGS += -DNRF_CRYPTO_MAX_INSTANCE_COUNT=1
COMMON_FLAGS += -DNRF_SD_BLE_API_VERSION=7
COMMON_FLAGS += -DSOFTDEVICE_PRESENT
COMMON_FLAGS += -DSWI_DISABLE0
COMMON_FLAGS += -DBOARD_CUSTOM
COMMON_FLAGS += -DBSP_SIMPLE=1
COMMON_FLAGS += -DUSE_APP_CONFIG=1
COMMON_FLAGS += -D__HEAP_SIZE=0 -D__STACK_SIZE=$(BES_STACK_SIZE)

ifeq ($(USE_NFC_AS_GPIO),1)
    $(info Using NFC pins as GPIOs)
	COMMON_FLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
endif

# C flags
CFLAGS += $(COMMON_FLAGS)
CFLAGS += -DMBEDTLS_CONFIG_FILE=\"nrf_crypto_mbedtls_config.h\"
CFLAGS += -Wall -Wextra -g3 -Wno-unused-parameter -Wno-expansion-to-defined -Wno-missing-field-initializers -Wno-array-bounds -Wimplicit-fallthrough=2

# keep every function in separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
# generate dependency output file
CFLAGS += -MP -MD

# C++ flags
# CXXFLAGS +=

# Assembler flags
ASMFLAGS += $(COMMON_FLAGS)

# Linker flags
LDFLAGS += -L$(NRF_SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
# let linker to dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs

# Enable float support for printf()
# LDFLAGS += -u _printf_float

# Target files definition

# list of files excluded form nRF5 SDK compilation in Makefile wildcard format
NRF_SDK_EXCLUDE_PATTERNS += %freertos.c %rtx.c %ant.c %bsp_btn_ble.c %_serconn.c %_KEIL.c %_IAR.c %_SES.c %_keil.c %_iar.c %_ses.c %app_uart.c %retarget.c %fstorage_nosd.c %nrf_crypto_svc.c %mbedtls_backend_aes.c

# list of Nordic SDK modules located at non-standard paths
NRF_SDK_DIRS += $(NRF_SDK_ROOT)/components/toolchain/cmsis/include $(NRF_SDK_ROOT)/integration/nrfx $(NRF_SDK_ROOT)/modules/nrfx
NRF_SDK_DIRS += $(NRF_SDK_ROOT)/modules/nrfx/drivers $(NRF_SDK_ROOT)/modules/nrfx/drivers/include $(NRF_SDK_ROOT)/modules/nrfx/hal
NRF_SDK_DIRS += $(NRF_SDK_ROOT)/modules/nrfx/soc $(NRF_SDK_ROOT)/components/softdevice/common

# add enabled module paths to the list
NRF_SDK_COMPONENT_DIRS += ble libraries drivers_nrf
NRF_SDK_DIRS += $(foreach component_dir,$(NRF_SDK_COMPONENT_DIRS),$(foreach dir,$(NRF_SDK_MODULES),$(sort $(dir $(wildcard $(NRF_SDK_ROOT)/components/$(component_dir)/$(dir)/)))))
NRF_SDK_DIRS += $(foreach dir,$(NRF_SDK_MODULES),$(sort $(dir $(wildcard $(NRF_SDK_ROOT)/external/$(dir)/))))

# autodetect Nordic nRF5 SDK source files
NRF_SDK_SRC_FILES := $(foreach dir,$(NRF_SDK_DIRS),$(wildcard $(dir)/*.c))
NRF_SDK_SRC_FILES += $(foreach fil,$(NRF_SDK_MODULES),$(wildcard $(NRF_SDK_ROOT)/modules/nrfx/drivers/src/$(fil).c))
NRF_SDK_SRC_FILES += $(foreach fil,$(NRF_SDK_MODULES),$(wildcard $(NRF_SDK_ROOT)/integration/nrfx/legacy/$(fil).c))

# filter out source files from exclude list
NRF_SDK_SRC_FILES := $(filter-out $(NRF_SDK_EXCLUDE_PATTERNS),$(NRF_SDK_SRC_FILES))

# autodetect project source and include directory list

# search path for project source and includes
PROJ_SEARCH_PATH:=$(PROJ_DIR)/proto/ $(PROJ_DIR)/src/ $(PROJ_DIR)/src/*/ $(PROJ_DIR)/config/

# expand search path to list of directories
PROJ_SRC_DIRS:=$(foreach dir,$(PROJ_SEARCH_PATH),$(sort $(dir $(wildcard $(dir)))))

# autodetect project source files
PROJ_SRC_FILES := $(foreach dir,$(PROJ_SRC_DIRS),$(wildcard $(dir)/*.c))

# add include only module paths to the list
NRF_SDK_DIRS += $(foreach dir,$(NRF_SDK_INCLUDE_ONLY_MODULES),$(sort $(dir $(wildcard $(NRF_SDK_ROOT)/components/*/$(dir)/))))
NRF_SDK_DIRS += $(foreach dir,$(NRF_SDK_INCLUDE_ONLY_MODULES),$(sort $(dir $(wildcard $(NRF_SDK_ROOT)/external/$(dir)/))))

# Nordic's Makefile.common assembles compilation procedure using INC_FOLDERS and SRC_FILES variables

# assemble include path
INC_FOLDERS += $(PROJ_SRC_DIRS) $(NRF_SDK_ROOT)/modules/nrfx/mdk $(NRF_SDK_ROOT)/integration/nrfx/legacy $(NRF_SDK_DIRS)

# assemble list of source files
SRC_FILES += $(NRF_SDK_SRC_FILES) $(PROJ_SRC_FILES)

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

# make target list
.PHONY: $(TARGETS) all clean help flash flash_softdevice

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo   MCU targets: $(TARGETS)
	@echo   flash: Upload compiled program to the mcu
	@echo   flash_softdevice: Upload $(notdir $(BES_SOFTDEVICE_HEX)) softdevice
	@echo Configuration variables:
	@echo   BUILD values: $(SUPPORTED_BUILDS)
	@echo   BOARD values: $(SUPPORTED_BOARDS)

TEMPLATE_PATH := $(NRF_SDK_ROOT)/components/toolchain/gcc

include $(TEMPLATE_PATH)/Makefile.common
$(foreach target, $(TARGETS), $(call define_target, $(target)))

# Wire generated protobuf files into Make's dependency chain
ifneq ($(PROTO_STAMP),)
$(foreach target,$(TARGETS),$(eval $(OUTPUT_DIRECTORY)/$(target)/$(PROTO_SRC_BASENAME).o: $(PROTO_STAMP)))
endif

# Flash the program
flash: $(OUTPUT_DIRECTORY)/$(.DEFAULT_GOAL).hex
	@echo Flashing: $<
	nrfjprog --program $< --sectorerase --reset

# Flash softdevice
flash_softdevice:
	@echo Flashing: $(notdir $(BES_SOFTDEVICE_HEX))
	nrfjprog --program $(BES_SOFTDEVICE_HEX) --chiperase --reset
