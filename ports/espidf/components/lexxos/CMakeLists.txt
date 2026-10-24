set(LEXXOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
set(LEXXOS_SRC_DIR "${LEXXOS_ROOT}/src")


set(LEXXOS_PLATFORM "espidf" CACHE STRING "Target platform backend" FORCE)
# ESP-IDF runs component CMakeLists.txt in an isolated requirements pass before
# the normal project configure stage. Cache values prepared by the root CMake
# entry point are not always visible there, so select a safe ESP-IDF board before
# including Boards.cmake. Otherwise Boards.cmake falls back to host_native and
# rewrites LEXXOS_PLATFORM to host during component discovery.
if (DEFINED LEXXOS_ESPIDF_BOARD
        AND NOT LEXXOS_ESPIDF_BOARD STREQUAL ""
        AND NOT LEXXOS_ESPIDF_BOARD STREQUAL "host_native")
    set(LEXXOS_BOARD "${LEXXOS_ESPIDF_BOARD}" CACHE STRING "Target board profile" FORCE)
elseif (DEFINED ENV{LEXXOS_ESPIDF_BOARD}
        AND NOT "$ENV{LEXXOS_ESPIDF_BOARD}" STREQUAL ""
        AND NOT "$ENV{LEXXOS_ESPIDF_BOARD}" STREQUAL "host_native")
    set(LEXXOS_BOARD "$ENV{LEXXOS_ESPIDF_BOARD}" CACHE STRING "Target board profile" FORCE)
elseif (DEFINED ENV{LEXXOS_BOARD}
        AND NOT "$ENV{LEXXOS_BOARD}" STREQUAL ""
        AND NOT "$ENV{LEXXOS_BOARD}" STREQUAL "host_native")
    set(LEXXOS_BOARD "$ENV{LEXXOS_BOARD}" CACHE STRING "Target board profile" FORCE)
elseif (NOT DEFINED LEXXOS_BOARD OR LEXXOS_BOARD STREQUAL "" OR LEXXOS_BOARD STREQUAL "host_native")
    if (DEFINED IDF_TARGET AND IDF_TARGET STREQUAL "esp32p4")
        set(LEXXOS_BOARD "jc_esp32p4_m3_dev" CACHE STRING "Target board profile" FORCE)
    elseif (DEFINED IDF_TARGET AND IDF_TARGET STREQUAL "esp32s3")
        set(LEXXOS_BOARD "m5stack_cardputer" CACHE STRING "Target board profile" FORCE)
    else ()
        set(LEXXOS_BOARD "m5stack_basic_v1" CACHE STRING "Target board profile" FORCE)
    endif ()
endif ()

include("${LEXXOS_ROOT}/cmake/Boards.cmake")

set(LEXXOS_BUILD_FLAVOR "standard" CACHE STRING "LexxOS feature preset")

if (LEXXOS_BOARD STREQUAL "m5stack_cardputer_adv")
    set(LEXXOS_ESPIDF_BOARD_SOURCE "${LEXXOS_SRC_DIR}/lexxos/platform/espidf/board_m5stack_cardputer_adv.cpp")
    set(LEXXOS_ESPIDF_BOARD_DISPLAY_DESCRIPTION "M5Stack Cardputer-Adv ST7789V2 240x135 SPI LCD")
elseif (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    set(LEXXOS_ESPIDF_BOARD_SOURCE "${LEXXOS_SRC_DIR}/lexxos/platform/espidf/board_jc_esp32p4_m3_dev.cpp")
    set(LEXXOS_ESPIDF_BOARD_DISPLAY_DESCRIPTION "Guition JC-ESP32P4-M3-DEV + BTT Pi TFT70 MIPI DSI display")
elseif (LEXXOS_BOARD STREQUAL "m5stack_cardputer")
    set(LEXXOS_ESPIDF_BOARD_SOURCE "${LEXXOS_SRC_DIR}/lexxos/platform/espidf/board_m5stack_cardputer.cpp")
    set(LEXXOS_ESPIDF_BOARD_DISPLAY_DESCRIPTION "M5Stack Cardputer ST7789V2 240x135 SPI LCD")
else ()
    set(LEXXOS_ESPIDF_BOARD_SOURCE "${LEXXOS_SRC_DIR}/lexxos/platform/espidf/board_m5stack_basic_v1.cpp")
    set(LEXXOS_ESPIDF_BOARD_DISPLAY_DESCRIPTION "M5Stack Basic 320x240 SPI LCD")
endif ()

# Hardware defaults come from Boards.cmake; build flavor and explicit preset
# options must survive component discovery without board-specific FORCE overrides.
set(LEXXOS_ESPIDF_USB_HOST_PORT "auto" CACHE STRING "ESP32-P4 USB Host port selection: auto, hs, fs or dual")

include("${LEXXOS_ROOT}/cmake/Features.cmake")
include("${LEXXOS_ROOT}/cmake/BuildInfo.cmake")

# Common LexxOS source groups are provided by the shared catalog used by
# the native CMake build and the ESP-IDF component.
if (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    set(LEXXOS_EFFECTIVE_SD_BACKEND "sdio")
else ()
    set(LEXXOS_EFFECTIVE_SD_BACKEND "spi")
endif ()
option(LEXXOS_ESPIDF_ENABLE_HOSTED "Build ESP-Hosted Wi-Fi/Bluetooth support" OFF)
if (NOT DEFINED LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
    set(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND ON CACHE BOOL "Build ESP-IDF Wi-Fi station backend")
endif ()
if (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev" AND NOT LEXXOS_ESPIDF_ENABLE_HOSTED)
    set(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND OFF CACHE BOOL "Build ESP-IDF Wi-Fi station backend" FORCE)
endif ()
if (LEXXOS_ESPIDF_ENABLE_HOSTED AND NOT LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
    message(FATAL_ERROR "LEXXOS_ESPIDF_ENABLE_HOSTED requires LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND")
endif ()
include("${LEXXOS_ROOT}/cmake/lexxos_sources.cmake")

set(LEXXOS_ESPIDF_EXTRA_REQUIRES)
if (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    list(APPEND LEXXOS_ESPIDF_EXTRA_REQUIRES espressif__usb_host_hid)
endif ()
if (LEXXOS_ESPIDF_ENABLE_HOSTED)
    list(APPEND LEXXOS_ESPIDF_EXTRA_REQUIRES espressif__esp_hosted)
endif ()
if (LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
    list(APPEND LEXXOS_ESPIDF_EXTRA_REQUIRES esp_wifi)
endif ()

set(LEXXOS_ESPIDF_BOARD_DISPLAY_SOURCES)
if (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    list(APPEND LEXXOS_ESPIDF_BOARD_DISPLAY_SOURCES
            "${LEXXOS_SRC_DIR}/lexxos/devices/espidf/mipi_dsi_display_espidf.cpp")
endif ()

idf_component_register(
        SRCS
        espidf_main.cpp
        ${LEXXOS_COMMON_SOURCES}
        ${LEXXOS_ESPIDF_BOARD_SOURCE}
        ${LEXXOS_SRC_DIR}/lexxos/kernel/espidf/scheduler_espidf.cpp
        ${LEXXOS_SRC_DIR}/lexxos/platform/espidf/platform_espidf.cpp
        ${LEXXOS_SRC_DIR}/lexxos/platform/espidf/detail/espidf_console.cpp
        ${LEXXOS_SRC_DIR}/lexxos/platform/espidf/detail/espidf_gpio.cpp
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/espidf_placeholder_i2c_bus.cpp
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/i2c_bus_espidf.cpp
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/m5stack_buttons_input_device.cpp
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/spi_bus_espidf.cpp
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/m5stack_lcd_display.cpp
        ${LEXXOS_ESPIDF_BOARD_DISPLAY_SOURCES}
        ${LEXXOS_SRC_DIR}/lexxos/devices/espidf/espidf_spi_bus.cpp
        INCLUDE_DIRS
        ${LEXXOS_SRC_DIR}
        ${LEXXOS_GENERATED_INCLUDE_DIR}
        REQUIRES
        driver
        esp_driver_gpio
        esp_driver_uart
        esp_driver_spi
        esp_driver_sdmmc
        esp_driver_i2c
        esp_driver_i2s
        esp_adc
        esp_eth
        esp_event
        esp_hw_support
        esp_lcd
        esp_netif
        esp_system
        esp_timer
        esp_driver_usb_serial_jtag
        heap
        freertos
        fatfs
        nvs_flash
        sdmmc
        spi_flash
        lwip
        ${LEXXOS_ESPIDF_EXTRA_REQUIRES}
        PRIV_REQUIRES
        esp_driver_rmt
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
        LEXXOS_PLATFORM_ESPIDF=1
        LEXXOS_KERNEL_ESPIDF=1
        LEXXOS_BOARD_NAME=\"${LEXXOS_BOARD}\"
)

string(TOLOWER "${LEXXOS_ESPIDF_USB_HOST_PORT}" LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED)
if (LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED STREQUAL "")
    set(LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED "auto")
endif ()
if (LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED STREQUAL "auto")
    set(LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED "hs")
endif ()
if (LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED STREQUAL "fs")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ESPIDF_USB_HOST_PORT_FS=1)
elseif (LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED STREQUAL "hs")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ESPIDF_USB_HOST_PORT_HS=1)
elseif (LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED STREQUAL "dual")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ESPIDF_USB_HOST_PORT_DUAL=1)
else ()
    message(FATAL_ERROR "Unsupported LEXXOS_ESPIDF_USB_HOST_PORT='${LEXXOS_ESPIDF_USB_HOST_PORT}'. Use auto, hs, fs or dual.")
endif ()
message(STATUS "LexxOS ESP-IDF USB host port: ${LEXXOS_ESPIDF_USB_HOST_PORT_NORMALIZED}")

if (LEXXOS_FEATURE_SD_CARD AND LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sdio")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_SD_BACKEND_SDIO=1)
endif ()

if (LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS=1)
endif ()

if (LEXXOS_ESPIDF_ENABLE_HOSTED)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ESPIDF_ENABLE_HOSTED=1)
endif ()

if (LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND=1)
endif ()

if (LEXXOS_ESPIDF_ENABLE_HOSTED AND LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND
        AND DEFINED IDF_VERSION_MAJOR AND DEFINED IDF_VERSION_MINOR)
    set(LEXXOS_WIFI_REMOTE_INJECTED_INCLUDE_DIR
            "${LEXXOS_ROOT}/managed_components/espressif__esp_wifi_remote/idf_v${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}/include/injected")
    if (EXISTS "${LEXXOS_WIFI_REMOTE_INJECTED_INCLUDE_DIR}/esp_wifi.h")
        target_include_directories(${COMPONENT_LIB} PRIVATE "${LEXXOS_WIFI_REMOTE_INJECTED_INCLUDE_DIR}")
    endif ()
endif ()

if (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LEXXOS_DISPLAY_SCRATCH_LINE_PIXELS=1024)
endif ()

# Honor the same explicit FatFs cache budget as native/Pico builds.
if (NOT LEXXOS_FAT_READ_CACHE_SECTORS STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
            LEXXOS_FAT_READ_CACHE_SECTORS=${LEXXOS_FAT_READ_CACHE_SECTORS})
endif ()

if (LEXXOS_FEATURE_GAMEBOY)
    set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS "${LEXXOS_GAMEBOY_ROM_CACHE_BANKS}")
    if (LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS STREQUAL "")
        set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS 4)
    endif ()

    set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES "${LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
            LEXXOS_GAMEBOY_ROM_CACHE_BANKS=${LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS}
            LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE=1
    )
    if (NOT LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES STREQUAL "")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
                LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=${LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES})
    endif ()

    set(LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET "${LEXXOS_GAMEBOY_FLASH_ROM_OFFSET}")
    if (NOT LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET STREQUAL "")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
                LEXXOS_GAMEBOY_FLASH_ROM_OFFSET=${LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET})
    endif ()

    if (LEXXOS_GAMEBOY_OPTIMIZE_HOTPATH)
        foreach (gameboy_source IN LISTS LEXXOS_GAMEBOY_SOURCES)
            set_source_files_properties(${gameboy_source}
                    PROPERTIES
                    COMPILE_OPTIONS "-O3")
        endforeach ()
    endif ()
endif ()

target_compile_options(${COMPONENT_LIB} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
