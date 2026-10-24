cmake_minimum_required(VERSION 3.20)

option(LEXXOS_RELEASE_ONLY "Configure release packaging without a firmware toolchain" OFF)
if (LEXXOS_RELEASE_ONLY)
    project(LexxOSRelease LANGUAGES NONE)
    include(cmake/Release.cmake)
    return()
endif ()

option(LEXXOS_APPS "Build standalone RV32I applications" ON)
option(LEXXOS_APPS_ONLY "Build only applications" OFF)
if (LEXXOS_APPS_ONLY)
    project(LexxOSApps LANGUAGES NONE)
    add_subdirectory(apps)
    include(cmake/Release.cmake)
    return()
endif ()

include(cmake/Version.cmake)
include(cmake/Boards.cmake)

set(LEXXOS_PLATFORM "host" CACHE STRING "Target platform backend: host, pico, sim, stm32f4, espidf")
set(LEXXOS_BOARD "host_native" CACHE STRING "Target board profile")
set(LEXXOS_PICO_USE_FREERTOS ON CACHE BOOL "Use FreeRTOS scheduler backend on Pico platform")
set(LEXXOS_PICO_FREERTOS_CORES 1 CACHE STRING "Number of FreeRTOS scheduler cores on Pico when the selected port supports SMP")
set(LEXXOS_EXPERIMENTAL_RP2350_SMP_FREERTOS OFF CACHE BOOL "Use the experimental RP2350 SMP FreeRTOS port; off keeps the stable single-core RP2350 port")
set(LEXXOS_PICO_USB_CDC_CONSOLE "auto" CACHE STRING "Enable Pico USB CDC console mirror: auto enables it for PicoCalc, on or off")
set(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE "factory" CACHE STRING "PicoCalc keyboard/southbridge firmware profile: factory or jcs")
set(LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE "" CACHE STRING "Optional PicoCalc system I2C baudrate; empty selects the keyboard firmware profile default")
set(LEXXOS_PICO_FREERTOS_HEAP_KB "" CACHE STRING "Optional Pico FreeRTOS heap size in KiB; empty selects an RP2040/RP2350 platform default")
set(LEXXOS_SD_IO_WORKER_CORE 1 CACHE STRING "Preferred SD I/O worker core when FreeRTOS core affinity is available")
set(LEXXOS_STM32F4_BRINGUP_ONLY OFF CACHE BOOL "Build only the minimal STM32F407 bring-up firmware instead of the LexxOS runtime")
set(LEXXOS_SD_BACKEND "" CACHE STRING "SD card backend override: empty selects platform default, spi or sdio")
set(LEXXOS_ESPIDF_USB_HOST_PORT "auto" CACHE STRING "ESP32-P4 USB Host port selection: auto, hs, fs or dual")
set(LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO OFF CACHE BOOL "Allow building the experimental RP2040 SDIO backend skeleton; SPI remains the production default")
set(LEXXOS_FAT_WRITE_BUFFER_SECTORS "" CACHE STRING "Number of 512-byte sectors accumulated before FatFs writes; empty selects a platform default")
set(LEXXOS_FAT_DISK_WRITE_MAX_SECTORS "" CACHE STRING "Maximum sector count passed from FatFs disk_write to the block device; 0 means unlimited, empty selects a platform default")
set(LEXXOS_FAT_READ_CACHE_SECTORS "" CACHE STRING "Number of 512-byte FAT disk read-ahead sectors; empty selects a platform default")
set(LEXXOS_FAT_SECTOR_CACHE_SLOTS "" CACHE STRING "Number of cached FAT metadata sectors; empty selects a platform default")
set(LEXXOS_FAT_CLUSTER_CACHE_ENTRIES "" CACHE STRING "Number of cached cluster-chain entries for the active FAT file; empty selects a platform default")
set(LEXXOS_FAT_MOUNT_SLOTS "" CACHE STRING "Number of simultaneous FAT mount slots; empty selects a platform default")
set(LEXXOS_PICO_SYS_CLOCK_KHZ "" CACHE STRING "Optional RP2040/RP2350 system clock in kHz; empty keeps the board default or Game Boy Pico 2 W performance default")
set(LEXXOS_ST7735_SPI_BAUDRATE "" CACHE STRING "Optional ST7735 SPI baudrate override for dev_pico; empty keeps board default")
set(LEXXOS_PICOCALC_SD_SPI_BAUDRATE "" CACHE STRING "Optional PicoCalc SD SPI baudrate override; empty keeps board default")
set(LEXXOS_PICOCALC_WIFI_AUTOCONNECT "" CACHE STRING "Optional PicoCalc boot Wi-Fi autoconnect override: empty selects default, 0 disables, 1 enables")
set(LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE "8000000" CACHE STRING "Optional PicoCalc PSRAM SPI baudrate; 0 selects a safe PIO default")
set(LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ "880" CACHE STRING "Default PicoCalc beep frequency in Hz")
set(LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS "120" CACHE STRING "Default PicoCalc beep duration in milliseconds")
set(LEXXOS_PICOCALC_LCD_SPI_BAUDRATE "" CACHE STRING "Optional PicoCalc LCD SPI baudrate override; empty keeps board default")
set(LEXXOS_ILI9488_RGB565_WIRE "auto" CACHE STRING "ILI9488 SPI wire format: auto enables RGB565 for known-compatible boards, on forces RGB565, off keeps RGB666")
set(LEXXOS_GAMEBOY_ROM_CACHE_BANKS "" CACHE STRING "Number of 16 KiB Game Boy ROM banks kept in RAM cache; empty selects a platform default")
set(LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES "" CACHE STRING "Game Boy ROM cache page size in bytes; empty selects the emulator default")
set(LEXXOS_GAMEBOY_OPTIMIZE_HOTPATH ON CACHE BOOL "Build Game Boy emulator hot path sources with speed-oriented options even in Debug presets")
set(LEXXOS_GAMEBOY_FLASH_ROM_OFFSET "" CACHE STRING "Flash offset used as optional Game Boy ROM staging area; empty selects a safe board default")
set(LEXXOS_GAMEBOY_ENABLE_LTO OFF CACHE BOOL "Enable link-time optimization for Game Boy performance builds")
set(LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE "40000000" CACHE STRING "Optional M5Stack Cardputer LCD SPI baudrate")
set(LEXXOS_CARDPUTER_LCD_MADCTL "0x60" CACHE STRING "Optional M5Stack Cardputer ST7789 MADCTL value")
set(LEXXOS_CARDPUTER_LCD_ADDRESS_WINDOW_SWAPS_AXES ON CACHE BOOL "Map Cardputer display logical landscape coordinates to native ST7789 portrait axes")
set(LEXXOS_CARDPUTER_SD_SPI_BAUDRATE "10000000" CACHE STRING "Optional M5Stack Cardputer/Cardputer-Adv microSD SPI baudrate")
set(LEXXOS_ENABLE_STATIC_CHECKS OFF CACHE BOOL "Enable opt-in repository hygiene checks for host CTest")
set(LEXXOS_HOST_NATIVE_ARCHITECTURE ON CACHE BOOL "Build host/sim macOS targets for the native host architecture")
set(LEXXOS_HOST_TARGET_ARCHITECTURE "" CACHE STRING "Optional macOS architecture override for host/sim targets; empty auto-selects native")

set_property(CACHE LEXXOS_ILI9488_RGB565_WIRE PROPERTY STRINGS auto on off)
set_property(CACHE LEXXOS_GAMEBOY_ROM_CACHE_BANKS PROPERTY STRINGS "" 1 2 3 4)
set_property(CACHE LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES PROPERTY STRINGS "" 2048 4096 8192 16384)
set_property(CACHE LEXXOS_FAT_READ_CACHE_SECTORS PROPERTY STRINGS "" 1 2 4 8 16 32 64)
set_property(CACHE LEXXOS_FAT_WRITE_BUFFER_SECTORS PROPERTY STRINGS "" 0 1 2 4 8 16 32)
set_property(CACHE LEXXOS_FAT_DISK_WRITE_MAX_SECTORS PROPERTY STRINGS "" 0 1 2 4 8 16 32)
set_property(CACHE LEXXOS_FAT_SECTOR_CACHE_SLOTS PROPERTY STRINGS "" 1 2 4)
set_property(CACHE LEXXOS_FAT_CLUSTER_CACHE_ENTRIES PROPERTY STRINGS "" 32 64 128 256)
set_property(CACHE LEXXOS_FAT_MOUNT_SLOTS PROPERTY STRINGS "" 1 2)
set_property(CACHE LEXXOS_PICO_SYS_CLOCK_KHZ PROPERTY STRINGS "" 125000 133000 150000 200000 250000 266000)
set_property(CACHE LEXXOS_PICO_USB_CDC_CONSOLE PROPERTY STRINGS auto on off)
set_property(CACHE LEXXOS_PICOCALC_KEYBOARD_FIRMWARE PROPERTY STRINGS factory jcs)
set_property(CACHE LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE PROPERTY STRINGS "" 10000 50000 100000 400000)
set_property(CACHE LEXXOS_PICOCALC_SD_SPI_BAUDRATE PROPERTY STRINGS "" 5000000 25000000 50000000 60000000)
set_property(CACHE LEXXOS_PICOCALC_LCD_SPI_BAUDRATE PROPERTY STRINGS "" 50000000 75000000)
set_property(CACHE LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE PROPERTY STRINGS "" 0 1000000 4000000 8000000 12000000 16000000 24000000 32000000)
set_property(CACHE LEXXOS_PICO_FREERTOS_HEAP_KB PROPERTY STRINGS "" 64 80 88 96 126 160 192 224 256 320)
set_property(CACHE LEXXOS_GAMEBOY_FLASH_ROM_OFFSET PROPERTY STRINGS "" 1048576 2097152 3145728)
set_property(CACHE LEXXOS_HOST_TARGET_ARCHITECTURE PROPERTY STRINGS "" arm64 x86_64)

set_property(CACHE LEXXOS_SD_BACKEND PROPERTY STRINGS "" spi sdio)
set_property(CACHE LEXXOS_ESPIDF_USB_HOST_PORT PROPERTY STRINGS auto hs fs dual)

set_property(CACHE LEXXOS_PICO_FREERTOS_CORES PROPERTY STRINGS 1 2)
set_property(CACHE LEXXOS_SD_IO_WORKER_CORE PROPERTY STRINGS 0 1)

if (LEXXOS_PLATFORM STREQUAL "espidf")
    include(cmake/platforms/espidf_prepare.cmake)
else ()
    include(cmake/platforms/native_prepare.cmake)
endif ()

project(LexxOS VERSION ${LEXXOS_VERSION} LANGUAGES C CXX ASM)

if (LEXXOS_APPS)
    add_subdirectory(apps)
endif ()

string(TOLOWER "${LEXXOS_PICOCALC_KEYBOARD_FIRMWARE}" LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_NORMALIZED)
if (NOT LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_NORMALIZED STREQUAL "factory"
        AND NOT LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_NORMALIZED STREQUAL "jcs")
    message(FATAL_ERROR "Unsupported LEXXOS_PICOCALC_KEYBOARD_FIRMWARE='${LEXXOS_PICOCALC_KEYBOARD_FIRMWARE}'. Use factory or jcs.")
endif ()

if (LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_NORMALIZED STREQUAL "jcs")
    add_compile_definitions(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS=1)
else ()
    add_compile_definitions(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_FACTORY=1)
endif ()

if (NOT LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE STREQUAL "")
    add_compile_definitions(LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE=${LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE})
endif ()

if (NOT LEXXOS_PICOCALC_LCD_SPI_BAUDRATE STREQUAL "")
    add_compile_definitions(LEXXOS_PICOCALC_LCD_SPI_BAUDRATE=${LEXXOS_PICOCALC_LCD_SPI_BAUDRATE})
endif ()

set(LEXXOS_PICO_WIFI_SYS_CLOCK_LIMIT_KHZ 250000)
set(LEXXOS_PICO_WIFI_BOARD OFF)
if (LEXXOS_PLATFORM STREQUAL "pico"
        AND (LEXXOS_BOARD STREQUAL "dev_pico2w"
        OR LEXXOS_BOARD STREQUAL "picocalc_pico_w"
        OR LEXXOS_BOARD STREQUAL "picocalc_pico2w"))
    set(LEXXOS_PICO_WIFI_BOARD ON)
endif ()
if (LEXXOS_PICO_WIFI_BOARD
        AND NOT LEXXOS_PICO_SYS_CLOCK_KHZ STREQUAL ""
        AND LEXXOS_PICO_SYS_CLOCK_KHZ GREATER LEXXOS_PICO_WIFI_SYS_CLOCK_LIMIT_KHZ)
    message(FATAL_ERROR
            "LEXXOS_PICO_SYS_CLOCK_KHZ=${LEXXOS_PICO_SYS_CLOCK_KHZ} is not allowed for "
            "Wi-Fi enabled Pico board LEXXOS_BOARD=${LEXXOS_BOARD}. CYW43 is hardware-tested up to "
            "${LEXXOS_PICO_WIFI_SYS_CLOCK_LIMIT_KHZ} kHz in LexxOS; use an empty value or "
            "LEXXOS_PICO_SYS_CLOCK_KHZ=${LEXXOS_PICO_WIFI_SYS_CLOCK_LIMIT_KHZ}.")
endif ()

if (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
    set(_lexxos_picocalc_pico_w_heap_kb "88")
    set(_lexxos_picocalc_pico_w_psram_baud "8000000")
    if (LEXXOS_PICO_FREERTOS_HEAP_KB STREQUAL "")
        set(LEXXOS_PICO_FREERTOS_HEAP_KB "${_lexxos_picocalc_pico_w_heap_kb}" CACHE STRING
                "Optional Pico FreeRTOS heap size in KiB; empty selects an RP2040/RP2350 platform default"
                FORCE)
    elseif (LEXXOS_PICO_FREERTOS_HEAP_KB GREATER _lexxos_picocalc_pico_w_heap_kb)
        message(WARNING
                "Reducing LEXXOS_PICO_FREERTOS_HEAP_KB=${LEXXOS_PICO_FREERTOS_HEAP_KB} to "
                "${_lexxos_picocalc_pico_w_heap_kb} for LEXXOS_BOARD=picocalc_pico_w; the RP2040 release profile "
                "must leave room for .bss, .heap and Pico SDK stack dummy sections.")
        set(LEXXOS_PICO_FREERTOS_HEAP_KB "${_lexxos_picocalc_pico_w_heap_kb}" CACHE STRING
                "Optional Pico FreeRTOS heap size in KiB; empty selects an RP2040/RP2350 platform default"
                FORCE)
    endif ()
    if (NOT LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE STREQUAL ""
            AND LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE GREATER _lexxos_picocalc_pico_w_psram_baud)
        message(WARNING
                "Reducing LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE=${LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE} to "
                "${_lexxos_picocalc_pico_w_psram_baud} for LEXXOS_BOARD=picocalc_pico_w; the RP2040 PSRAM PIO path "
                "is release-capped at the hardware-confirmed 8 MHz profile.")
        set(LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE "${_lexxos_picocalc_pico_w_psram_baud}" CACHE STRING
                "Optional PicoCalc PSRAM SPI baudrate; 0 selects a safe PIO default"
                FORCE)
    endif ()
endif ()

if (NOT LEXXOS_PICO_FREERTOS_HEAP_KB STREQUAL "")
    add_compile_definitions(LEXXOS_PICO_FREERTOS_HEAP_KB=${LEXXOS_PICO_FREERTOS_HEAP_KB})
endif ()

if (NOT LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE STREQUAL "")
    add_compile_definitions(LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE=${LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE})
endif ()


if (NOT LEXXOS_CARDPUTER_LCD_MADCTL STREQUAL "")
    add_compile_definitions(LEXXOS_CARDPUTER_LCD_MADCTL=${LEXXOS_CARDPUTER_LCD_MADCTL})
endif ()

if (NOT LEXXOS_CARDPUTER_SD_SPI_BAUDRATE STREQUAL "")
    add_compile_definitions(LEXXOS_CARDPUTER_SD_SPI_BAUDRATE=${LEXXOS_CARDPUTER_SD_SPI_BAUDRATE})
endif ()

if (LEXXOS_CARDPUTER_LCD_ADDRESS_WINDOW_SWAPS_AXES)
    add_compile_definitions(LEXXOS_CARDPUTER_LCD_ADDRESS_WINDOW_SWAPS_AXES=1)
endif ()

if (LEXXOS_PLATFORM STREQUAL "espidf")
    include(cmake/platforms/espidf_project.cmake)
else ()
    include(cmake/platforms/native_project.cmake)
endif ()

include(cmake/Release.cmake)
