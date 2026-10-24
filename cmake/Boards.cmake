include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)

set(LEXXOS_BOARD "host_native" CACHE STRING "Target board profile")
set(LEXXOS_PLATFORM "host" CACHE STRING "Target platform backend derived from the selected LexxOS board")

set_property(CACHE LEXXOS_PLATFORM PROPERTY STRINGS
        host
        pico
        sim
        stm32f4
        espidf
)

set(_LEXXOS_BOARD_IDS
        host_native
        dev_pico
        dev_pico_sdio
        dev_pico2w
        picocalc_pico_w
        picocalc_pico2w
        dev-sim
        euse_m4_demo_stm32f407
        m5stack_basic_v1
        m5stack_cardputer
        m5stack_cardputer_adv
        jc_esp32p4_m3_dev
        picocalc_pico
        picocalc_pico2
)
set(_LEXXOS_BOARD_OPTIONS
        LEXXOS_BOARD_HOST_NATIVE
        LEXXOS_BOARD_DEV_PICO
        LEXXOS_BOARD_DEV_PICO_SDIO
        LEXXOS_BOARD_DEV_PICO2W
        LEXXOS_BOARD_PICOCALC_PICO_W
        LEXXOS_BOARD_PICOCALC_PICO2W
        LEXXOS_BOARD_DEV_SIM
        LEXXOS_BOARD_EUSE_M4_DEMO_STM32F407
        LEXXOS_BOARD_M5STACK_BASIC_V1
        LEXXOS_BOARD_M5STACK_CARDPUTER
        LEXXOS_BOARD_M5STACK_CARDPUTER_ADV
        LEXXOS_BOARD_JC_ESP32P4_M3_DEV
        LEXXOS_BOARD_PICOCALC_PICO
        LEXXOS_BOARD_PICOCALC_PICO2
)
set(_LEXXOS_BOARD_PLATFORMS
        host
        pico
        pico
        pico
        pico
        pico
        sim
        stm32f4
        espidf
        espidf
        espidf
        espidf
        pico
        pico
)
set(_LEXXOS_BOARD_FAMILIES
        HOST_NATIVE
        DEV_PICO
        DEV_PICO
        DEV_PICO
        PICOCALC
        PICOCALC
        SIM
        EUSE_M4_DEMO_STM32F407
        M5STACK_BASIC
        CARDPUTER
        CARDPUTER
        JC_ESP32P4_M3_DEV
        PICOCALC
        PICOCALC
)
set(_LEXXOS_BOARD_PICO_SDK_BOARDS
        ""
        pico
        pico
        pico2_w
        pico_w
        pico2_w
        ""
        ""
        ""
        ""
        ""
        ""
        pico
        pico2
)

set_property(CACHE LEXXOS_BOARD PROPERTY STRINGS ${_LEXXOS_BOARD_IDS})


function(_lexxos_board_option_description option output_variable)
    if (option STREQUAL "LEXXOS_BOARD_HOST_NATIVE")
        set(${output_variable} "Build for the native host board profile" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_DEV_PICO")
        set(${output_variable} "Build for the LexxOS Raspberry Pi Pico development board" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_DEV_PICO_SDIO")
        set(${output_variable} "Build for the LexxOS Raspberry Pi Pico SDIO development board profile" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_DEV_PICO2W")
        set(${output_variable} "Build for the LexxOS Raspberry Pi Pico 2 W development board" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_PICOCALC_PICO_W")
        set(${output_variable} "Build for ClockworkPi PicoCalc with Raspberry Pi Pico W" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_PICOCALC_PICO2W")
        set(${output_variable} "Build for ClockworkPi PicoCalc with Raspberry Pi Pico 2 W" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_DEV_SIM")
        set(${output_variable} "Build the development board simulator profile" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_EUSE_M4_DEMO_STM32F407")
        set(${output_variable} "Build for Euse M4 Demo STM32F407ZG" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_M5STACK_BASIC_V1")
        set(${output_variable} "Build for M5Stack Basic v1" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_M5STACK_CARDPUTER")
        set(${output_variable} "Build for M5Stack Cardputer" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_M5STACK_CARDPUTER_ADV")
        set(${output_variable} "Build for M5Stack Cardputer-Adv" PARENT_SCOPE)
    elseif (option STREQUAL "LEXXOS_BOARD_JC_ESP32P4_M3_DEV")
        set(${output_variable} "Build for Guition JC-ESP32P4-M3-DEV" PARENT_SCOPE)
    else ()
        set(${output_variable} "Build for ${option}" PARENT_SCOPE)
    endif ()
endfunction()

foreach (_lexxos_board_option IN LISTS _LEXXOS_BOARD_OPTIONS)
    _lexxos_board_option_description(${_lexxos_board_option} _lexxos_board_option_help)
    option(${_lexxos_board_option} "${_lexxos_board_option_help}" OFF)
endforeach ()

set(_lexxos_selected_board_options)
set(_lexxos_selected_board_ids)
list(LENGTH _LEXXOS_BOARD_OPTIONS _lexxos_board_count)
math(EXPR _lexxos_last_board_index "${_lexxos_board_count} - 1")
foreach (_lexxos_board_index RANGE 0 ${_lexxos_last_board_index})
    list(GET _LEXXOS_BOARD_OPTIONS ${_lexxos_board_index} _lexxos_board_option)
    list(GET _LEXXOS_BOARD_IDS ${_lexxos_board_index} _lexxos_board_id)
    if (${_lexxos_board_option})
        list(APPEND _lexxos_selected_board_options ${_lexxos_board_option})
        list(APPEND _lexxos_selected_board_ids ${_lexxos_board_id})
    endif ()
endforeach ()

list(LENGTH _lexxos_selected_board_options _lexxos_selected_board_option_count)
if (_lexxos_selected_board_option_count GREATER 1)
    string(REPLACE ";" ", " _lexxos_selected_board_options_text "${_lexxos_selected_board_options}")
    message(WARNING "Only one LexxOS board may be active. Selected board options: ${_lexxos_selected_board_options_text}")
    message(FATAL_ERROR "Disable all but one LEXXOS_BOARD_* option and configure again.")
elseif (_lexxos_selected_board_option_count EQUAL 1)
    list(GET _lexxos_selected_board_ids 0 _lexxos_selected_board)
elseif (LEXXOS_BOARD STREQUAL "")
    set(_lexxos_selected_board "host_native")
else ()
    set(_lexxos_selected_board "${LEXXOS_BOARD}")
endif ()

list(FIND _LEXXOS_BOARD_IDS "${_lexxos_selected_board}" _lexxos_selected_board_index)
if (_lexxos_selected_board_index EQUAL -1)
    string(REPLACE ";" ", " _lexxos_supported_boards_text "${_LEXXOS_BOARD_IDS}")
    message(FATAL_ERROR "Unsupported LEXXOS_BOARD='${_lexxos_selected_board}'. Supported boards: ${_lexxos_supported_boards_text}")
endif ()

list(GET _LEXXOS_BOARD_PLATFORMS ${_lexxos_selected_board_index} _lexxos_selected_platform)
list(GET _LEXXOS_BOARD_FAMILIES ${_lexxos_selected_board_index} _lexxos_selected_family)
list(GET _LEXXOS_BOARD_OPTIONS ${_lexxos_selected_board_index} _lexxos_selected_board_option)

if (NOT LEXXOS_BOARD STREQUAL "${_lexxos_selected_board}")
    message(STATUS "Selecting LEXXOS_BOARD=${_lexxos_selected_board}")
endif ()
set(LEXXOS_BOARD "${_lexxos_selected_board}" CACHE STRING "Target board profile" FORCE)

if (NOT LEXXOS_PLATFORM STREQUAL "${_lexxos_selected_platform}")
    message(STATUS "Selecting LEXXOS_PLATFORM=${_lexxos_selected_platform} for LEXXOS_BOARD=${LEXXOS_BOARD}")
endif ()
set(LEXXOS_PLATFORM "${_lexxos_selected_platform}" CACHE STRING "Target platform backend derived from the selected LexxOS board" FORCE)
set(LEXXOS_BOARD_PLATFORM "${_lexxos_selected_platform}" CACHE INTERNAL "Platform required by the selected LexxOS board" FORCE)
set(LEXXOS_BOARD_FAMILY "${_lexxos_selected_family}" CACHE INTERNAL "Board family of the selected LexxOS board" FORCE)

foreach (_lexxos_board_option IN LISTS _LEXXOS_BOARD_OPTIONS)
    _lexxos_board_option_description(${_lexxos_board_option} _lexxos_board_option_help)
    if (_lexxos_board_option STREQUAL _lexxos_selected_board_option)
        set(${_lexxos_board_option} ON CACHE BOOL "${_lexxos_board_option_help}" FORCE)
    else ()
        set(${_lexxos_board_option} OFF CACHE BOOL "${_lexxos_board_option_help}" FORCE)
    endif ()
endforeach ()

foreach (_lexxos_board_family IN ITEMS HOST_NATIVE DEV_PICO PICOCALC SIM EUSE_M4_DEMO_STM32F407 M5STACK_BASIC CARDPUTER JC_ESP32P4_M3_DEV)
    set(_lexxos_board_family_enabled OFF)
    if (_lexxos_selected_family STREQUAL _lexxos_board_family)
        set(_lexxos_board_family_enabled ON)
    endif ()
    set(LEXXOS_BOARD_FAMILY_${_lexxos_board_family} ${_lexxos_board_family_enabled} CACHE INTERNAL "Selected LexxOS board family flag" FORCE)
endforeach ()

foreach (_lexxos_board_index RANGE 0 ${_lexxos_last_board_index})
    list(GET _LEXXOS_BOARD_OPTIONS ${_lexxos_board_index} _lexxos_board_option)
    if (_lexxos_board_option STREQUAL _lexxos_selected_board_option)
        set(_lexxos_board_enabled_value ON)
    else ()
        set(_lexxos_board_enabled_value OFF)
    endif ()
    set(${_lexxos_board_option}_ACTIVE ${_lexxos_board_enabled_value} CACHE INTERNAL "Generated active board flag" FORCE)
endforeach ()

set(LEXXOS_BOARD_CONTROLLED_FEATURES
        DISPLAY
        DEBUG_DISPLAY
        KEYBOARD
        SD_CARD
        FATFS
        DS3231_RTC
        PSRAM
        GPIO_ANALYZER
        CACHE INTERNAL "Features controlled by the selected board profile"
)

function(_lexxos_set_board_feature_matrix available_features default_features)
    foreach (_lexxos_board_feature IN LISTS LEXXOS_BOARD_CONTROLLED_FEATURES)
        list(FIND available_features ${_lexxos_board_feature} _lexxos_available_index)
        list(FIND default_features ${_lexxos_board_feature} _lexxos_default_index)

        if (_lexxos_available_index EQUAL -1)
            set(LEXXOS_BOARD_AVAILABLE_${_lexxos_board_feature} OFF CACHE INTERNAL "Feature availability for selected board" FORCE)
        else ()
            set(LEXXOS_BOARD_AVAILABLE_${_lexxos_board_feature} ON CACHE INTERNAL "Feature availability for selected board" FORCE)
        endif ()

        if (_lexxos_default_index EQUAL -1)
            set(LEXXOS_BOARD_DEFAULT_${_lexxos_board_feature} OFF CACHE INTERNAL "Default feature state for selected board" FORCE)
        else ()
            set(LEXXOS_BOARD_DEFAULT_${_lexxos_board_feature} ON CACHE INTERNAL "Default feature state for selected board" FORCE)
        endif ()
    endforeach ()
endfunction()

set(_lexxos_board_available_features)
set(_lexxos_board_default_features)

if (LEXXOS_BOARD STREQUAL "host_native")
    set(_lexxos_board_available_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC)
elseif (LEXXOS_BOARD STREQUAL "dev_pico" OR LEXXOS_BOARD STREQUAL "dev_pico_sdio" OR LEXXOS_BOARD STREQUAL "dev_pico2w")
    set(_lexxos_board_available_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC)
elseif (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM)
elseif (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM GPIO_ANALYZER)
elseif (LEXXOS_BOARD STREQUAL "dev-sim")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS)
elseif (LEXXOS_BOARD STREQUAL "euse_m4_demo_stm32f407")
    set(_lexxos_board_available_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC GPIO_ANALYZER)
    set(_lexxos_board_default_features DISPLAY DEBUG_DISPLAY KEYBOARD SD_CARD FATFS DS3231_RTC)
elseif (LEXXOS_BOARD STREQUAL "m5stack_basic_v1")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS)
elseif (LEXXOS_BOARD STREQUAL "m5stack_cardputer")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS)
elseif (LEXXOS_BOARD STREQUAL "m5stack_cardputer_adv")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS)
elseif (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    set(_lexxos_board_available_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM)
    set(_lexxos_board_default_features DISPLAY KEYBOARD SD_CARD FATFS PSRAM)
endif ()

_lexxos_set_board_feature_matrix("${_lexxos_board_available_features}" "${_lexxos_board_default_features}")

if (LEXXOS_PLATFORM STREQUAL "pico")
    set(PICO_BOARD "${PICO_BOARD}" CACHE STRING "Raspberry Pi Pico SDK board")
    set_property(CACHE PICO_BOARD PROPERTY STRINGS pico pico_w pico2 pico2_w)

    list(GET _LEXXOS_BOARD_PICO_SDK_BOARDS ${_lexxos_selected_board_index} _lexxos_required_pico_board)
    if (NOT _lexxos_required_pico_board STREQUAL "" AND NOT PICO_BOARD STREQUAL "${_lexxos_required_pico_board}")
        message(STATUS "Selecting PICO_BOARD=${_lexxos_required_pico_board} for LEXXOS_BOARD=${LEXXOS_BOARD}")
        set(PICO_BOARD "${_lexxos_required_pico_board}" CACHE STRING "Raspberry Pi Pico SDK board" FORCE)
    endif ()
endif ()

set(LEXXOS_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include" CACHE INTERNAL "LexxOS generated include directory")
file(MAKE_DIRECTORY "${LEXXOS_GENERATED_INCLUDE_DIR}")

set(LEXXOS_BOARD_FILE "${LEXXOS_GENERATED_INCLUDE_DIR}/board_list.h")
file(WRITE "${LEXXOS_BOARD_FILE}" "#pragma once\n\n/*\n * This file is auto-generated by cmake/Boards.cmake.\n * Do not edit this manually.\n * Do not include this directly, use #include <lexxos/board_config.h> instead.\n */\n\n")

foreach (_lexxos_board_index RANGE 0 ${_lexxos_last_board_index})
    list(GET _LEXXOS_BOARD_OPTIONS ${_lexxos_board_index} _lexxos_board_option)
    if (_lexxos_board_option STREQUAL _lexxos_selected_board_option)
        set(_lexxos_board_value 1)
    else ()
        set(_lexxos_board_value "(-1)")
    endif ()
    file(APPEND "${LEXXOS_BOARD_FILE}" "#define ${_lexxos_board_option} ${_lexxos_board_value}\n")
endforeach ()

file(APPEND "${LEXXOS_BOARD_FILE}" "\n")
foreach (_lexxos_board_family IN ITEMS HOST_NATIVE DEV_PICO PICOCALC SIM EUSE_M4_DEMO_STM32F407 M5STACK_BASIC CARDPUTER JC_ESP32P4_M3_DEV)
    if (LEXXOS_BOARD_FAMILY_${_lexxos_board_family})
        set(_lexxos_board_family_value 1)
    else ()
        set(_lexxos_board_family_value "(-1)")
    endif ()
    file(APPEND "${LEXXOS_BOARD_FILE}" "#define LEXXOS_BOARD_FAMILY_${_lexxos_board_family} ${_lexxos_board_family_value}\n")
endforeach ()

message(STATUS "LEXXOS_BOARD: ${LEXXOS_BOARD}")
message(STATUS "LEXXOS_PLATFORM: ${LEXXOS_PLATFORM}")
message(STATUS "LEXXOS_BOARD_FAMILY: ${LEXXOS_BOARD_FAMILY}")
