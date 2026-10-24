include_guard(GLOBAL)

set(LEXXOS_BUILD_FLAVOR "standard" CACHE STRING "LexxOS feature preset: minimal, standard or full")
set_property(CACHE LEXXOS_BUILD_FLAVOR PROPERTY STRINGS minimal standard full)
option(LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES
        "Allow known high-SRAM experimental features on ClockworkPi PicoCalc with Raspberry Pi Pico W/RP2040"
        OFF)

set(LEXXOS_FULL_FEATURES_DEFAULT OFF)
if (LEXXOS_BUILD_FLAVOR STREQUAL "full")
    set(LEXXOS_FULL_FEATURES_DEFAULT ON)
elseif (NOT LEXXOS_BUILD_FLAVOR STREQUAL "standard" AND NOT LEXXOS_BUILD_FLAVOR STREQUAL "minimal")
    message(FATAL_ERROR "Unsupported LEXXOS_BUILD_FLAVOR='${LEXXOS_BUILD_FLAVOR}', expected minimal, standard or full")
endif ()

function(lexxos_board_feature_default feature_name fallback_value output_variable)
    if (DEFINED LEXXOS_BOARD_DEFAULT_${feature_name})
        set(${output_variable} ${LEXXOS_BOARD_DEFAULT_${feature_name}} PARENT_SCOPE)
    else ()
        set(${output_variable} ${fallback_value} PARENT_SCOPE)
    endif ()
endfunction()

lexxos_board_feature_default(DISPLAY ON LEXXOS_FEATURE_DISPLAY_DEFAULT)
lexxos_board_feature_default(DEBUG_DISPLAY ${LEXXOS_FEATURE_DISPLAY_DEFAULT} LEXXOS_FEATURE_DEBUG_DISPLAY_DEFAULT)
lexxos_board_feature_default(KEYBOARD ON LEXXOS_FEATURE_KEYBOARD_DEFAULT)
lexxos_board_feature_default(SD_CARD ON LEXXOS_FEATURE_SD_CARD_DEFAULT)
lexxos_board_feature_default(FATFS ON LEXXOS_FEATURE_FATFS_DEFAULT)
lexxos_board_feature_default(DS3231_RTC ON LEXXOS_FEATURE_DS3231_RTC_DEFAULT)
lexxos_board_feature_default(PSRAM OFF LEXXOS_FEATURE_PSRAM_DEFAULT)
lexxos_board_feature_default(GPIO_ANALYZER OFF LEXXOS_FEATURE_GPIO_ANALYZER_DEFAULT)


set(LEXXOS_HEALTH_DEFAULT ON)
if (LEXXOS_BUILD_FLAVOR STREQUAL "minimal")
    set(LEXXOS_HEALTH_DEFAULT OFF)
    set(LEXXOS_FEATURE_DEBUG_DISPLAY_DEFAULT OFF)
    set(LEXXOS_FEATURE_GPIO_ANALYZER_DEFAULT OFF)
elseif (LEXXOS_BUILD_FLAVOR STREQUAL "full")
    set(LEXXOS_FEATURE_DEBUG_DISPLAY_DEFAULT OFF)
    if (LEXXOS_BOARD_AVAILABLE_GPIO_ANALYZER)
        set(LEXXOS_FEATURE_GPIO_ANALYZER_DEFAULT ON)
    endif ()
endif ()

option(LEXXOS_FEATURE_DISPLAY "Build display drivers and display device registration" ${LEXXOS_FEATURE_DISPLAY_DEFAULT})

option(LEXXOS_FEATURE_DS3231_RTC "Build DS3231-compatible external RTC backend" ${LEXXOS_FEATURE_DS3231_RTC_DEFAULT})
option(LEXXOS_FEATURE_DEBUG_DISPLAY "Build the legacy I2C debug/status display task" ${LEXXOS_FEATURE_DEBUG_DISPLAY_DEFAULT})
option(LEXXOS_FEATURE_HEALTH "Enable health task" ${LEXXOS_HEALTH_DEFAULT})
option(LEXXOS_FEATURE_SD_CARD "Build SD card block device and SD-aware storage commands" ${LEXXOS_FEATURE_SD_CARD_DEFAULT})
option(LEXXOS_FEATURE_FATFS "Build FatFs as the default FAT filesystem backend" ${LEXXOS_FEATURE_FATFS_DEFAULT})
option(LEXXOS_FEATURE_MIDI "Build experimental MIDI playback commands" ${LEXXOS_FULL_FEATURES_DEFAULT})
option(LEXXOS_FEATURE_KEYBOARD "Build optional matrix/PCF8575/sim input adapters; board-family keyboards are gated separately" ${LEXXOS_FEATURE_KEYBOARD_DEFAULT})
option(LEXXOS_FEATURE_PSRAM "Build PSRAM-backed program memory arena support" ${LEXXOS_FEATURE_PSRAM_DEFAULT})
option(LEXXOS_FEATURE_GPIO_ANALYZER "Build the interactive digital GPIO analyzer program" ${LEXXOS_FEATURE_GPIO_ANALYZER_DEFAULT})
# Experimental analyzer is shipped only in full, including reused CMake caches.
if (NOT LEXXOS_BUILD_FLAVOR STREQUAL "full")
    set(LEXXOS_FEATURE_GPIO_ANALYZER OFF CACHE BOOL
            "Build the experimental digital GPIO analyzer (full only)" FORCE)
endif ()

option(LEXXOS_FEATURE_TEST_COMMANDS "Build all optional hardware test commands" ${LEXXOS_FULL_FEATURES_DEFAULT})
include("${CMAKE_CURRENT_LIST_DIR}/CommandOptions.cmake")
option(LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS "Build SD card test/diagnostic commands" ${LEXXOS_COMMANDS_TESTS})
option(LEXXOS_FEATURE_DISPLAY_TEST_COMMANDS "Build display test commands" ${LEXXOS_COMMANDS_TESTS})
option(LEXXOS_FEATURE_KEYBOARD_TEST_COMMANDS "Build keyboard/input test commands" ${LEXXOS_COMMANDS_TESTS})
lexxos_apply_command_policy()

if (LEXXOS_BUILD_FLAVOR STREQUAL "full")
    set(LEXXOS_FEATURE_GAMEBOY_DEFAULT ON)
elseif (DEFINED LEXXOS_ENABLE_GAMEBOY)
    set(LEXXOS_FEATURE_GAMEBOY_DEFAULT ${LEXXOS_ENABLE_GAMEBOY})
else ()
    set(LEXXOS_FEATURE_GAMEBOY_DEFAULT OFF)
endif ()

option(LEXXOS_FEATURE_GAMEBOY "Build Game Boy emulator programs" ${LEXXOS_FEATURE_GAMEBOY_DEFAULT})
if (DEFINED LEXXOS_ENABLE_GAMEBOY)
    set(LEXXOS_FEATURE_GAMEBOY ${LEXXOS_ENABLE_GAMEBOY} CACHE BOOL "Build Game Boy emulator programs" FORCE)
endif ()

function(lexxos_disable_picocalc_pico_w_heavy_feature feature_name reason)
    if (LEXXOS_FEATURE_${feature_name})
        message(WARNING
                "Disabling LEXXOS_FEATURE_${feature_name}=ON for LEXXOS_BOARD=picocalc_pico_w: ${reason}. "
                "Set LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES=ON only for an explicit experimental RP2040 build.")
        set(LEXXOS_FEATURE_${feature_name} OFF CACHE BOOL "Disabled by the PicoCalc Pico W RP2040 SRAM profile" FORCE)
    endif ()
endfunction()

if ((LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico") AND NOT LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES)
    lexxos_disable_picocalc_pico_w_heavy_feature(GAMEBOY
            "Game Boy programs add ROM, PPU and cartridge staging buffers that do not fit the release Pico W profile")
    lexxos_disable_picocalc_pico_w_heavy_feature(MIDI
            "MIDI playback is still experimental and competes with Wi-Fi, display and SD/FAT for RP2040 SRAM")
    lexxos_disable_picocalc_pico_w_heavy_feature(GPIO_ANALYZER
            "GPIO analyzer is an interactive diagnostic, not part of the first-generation Pico W release profile")
endif ()

set(LEXXOS_ENABLE_GAMEBOY ${LEXXOS_FEATURE_GAMEBOY} CACHE BOOL "Compatibility alias for LEXXOS_FEATURE_GAMEBOY" FORCE)

foreach (LEXXOS_BOARD_CONTROLLED_FEATURE IN LISTS LEXXOS_BOARD_CONTROLLED_FEATURES)
    if (LEXXOS_FEATURE_${LEXXOS_BOARD_CONTROLLED_FEATURE}
            AND DEFINED LEXXOS_BOARD_AVAILABLE_${LEXXOS_BOARD_CONTROLLED_FEATURE}
            AND NOT LEXXOS_BOARD_AVAILABLE_${LEXXOS_BOARD_CONTROLLED_FEATURE})
        message(FATAL_ERROR
                "LEXXOS_FEATURE_${LEXXOS_BOARD_CONTROLLED_FEATURE}=ON is not available on LEXXOS_BOARD=${LEXXOS_BOARD}")
    endif ()
endforeach ()

# FAT backend is optional: some bring-up stages intentionally expose only the
# raw SD block device. Mount/automount programs are guarded by FAT feature flags.

if (LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS AND NOT LEXXOS_FEATURE_SD_CARD)
    message(FATAL_ERROR "LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS requires LEXXOS_FEATURE_SD_CARD")
endif ()

if (LEXXOS_FEATURE_DEBUG_DISPLAY AND NOT LEXXOS_FEATURE_DISPLAY)
    message(FATAL_ERROR "LEXXOS_FEATURE_DEBUG_DISPLAY requires LEXXOS_FEATURE_DISPLAY")
endif ()

if (LEXXOS_FEATURE_DISPLAY_TEST_COMMANDS AND NOT LEXXOS_FEATURE_DISPLAY)
    message(FATAL_ERROR "LEXXOS_FEATURE_DISPLAY_TEST_COMMANDS requires LEXXOS_FEATURE_DISPLAY")
endif ()

if (LEXXOS_FEATURE_KEYBOARD_TEST_COMMANDS AND NOT LEXXOS_FEATURE_KEYBOARD)
    message(FATAL_ERROR "LEXXOS_FEATURE_KEYBOARD_TEST_COMMANDS requires LEXXOS_FEATURE_KEYBOARD")
endif ()

if (LEXXOS_FEATURE_GPIO_ANALYZER AND NOT LEXXOS_FEATURE_DISPLAY)
    message(FATAL_ERROR "LEXXOS_FEATURE_GPIO_ANALYZER requires LEXXOS_FEATURE_DISPLAY")
endif ()

if (LEXXOS_FEATURE_GAMEBOY AND NOT LEXXOS_FEATURE_PSRAM AND NOT LEXXOS_PLATFORM STREQUAL "espidf")
    message(WARNING "LEXXOS_FEATURE_GAMEBOY=ON but LEXXOS_FEATURE_PSRAM=OFF; gbemu requires external program memory and will be registered disabled. Other Game Boy diagnostic programs remain available.")
endif ()

# Transitional source compatibility: board selectors were moved out of the
# feature namespace, but older source files still use FEATURE_ENABLED(PICO_CALC)
# or FEATURE_ENABLED(CARDPUTER). Keep these as generated aliases only; do not
# expose them as CMake feature options.
set(LEXXOS_FEATURES
        DEBUG_DISPLAY
        HEALTH
        SD_CARD
        FATFS
        MIDI
        DISPLAY
        KEYBOARD
        SD_CARD_TEST_COMMANDS
        DISPLAY_TEST_COMMANDS
        KEYBOARD_TEST_COMMANDS
        DS3231_RTC
        GAMEBOY
        PSRAM
        GPIO_ANALYZER
)

set(LEXXOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include" CACHE INTERNAL "LexxOS generated include directory")
file(MAKE_DIRECTORY "${LEXXOS_GENERATED_INCLUDE_DIR}")

set(LEXXOS_FEATURE_FILE "${LEXXOS_GENERATED_INCLUDE_DIR}/feature_list.h")
file(WRITE "${LEXXOS_FEATURE_FILE}" "#pragma once\n\n/*\n * This file is auto-generated by cmake/Features.cmake.\n * Do not edit this manually.\n * Do not include this directly, use #include <lexxos/feature.h> instead.\n */\n\n#include <board_list.h>\n\n")

foreach (FEATURE IN LISTS LEXXOS_FEATURES)
    set(OPTION_NAME "LEXXOS_FEATURE_${FEATURE}")
    if (${OPTION_NAME})
        set(FEATURE_VALUE 1)
    else ()
        set(FEATURE_VALUE "(-1)")
    endif ()
    file(APPEND "${LEXXOS_FEATURE_FILE}" "#define FEATURE_${FEATURE} ${FEATURE_VALUE}\n")
    message(STATUS "FEATURE_${FEATURE}: ${${OPTION_NAME}}")
endforeach ()

file(APPEND "${LEXXOS_FEATURE_FILE}" "\n")
file(APPEND "${LEXXOS_FEATURE_FILE}" "/* Board-family compatibility aliases. Prefer <lexxos/board_config.h> in new code. */\n")
file(APPEND "${LEXXOS_FEATURE_FILE}" "#define FEATURE_PICO_CALC LEXXOS_BOARD_FAMILY_PICOCALC\n")
file(APPEND "${LEXXOS_FEATURE_FILE}" "#define FEATURE_CARDPUTER LEXXOS_BOARD_FAMILY_CARDPUTER\n")
message(STATUS "BOARD_FAMILY_PICOCALC: ${LEXXOS_BOARD_FAMILY_PICOCALC}")
message(STATUS "BOARD_FAMILY_CARDPUTER: ${LEXXOS_BOARD_FAMILY_CARDPUTER}")
