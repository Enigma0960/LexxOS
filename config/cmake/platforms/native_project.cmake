include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)

if (NOT LEXXOS_SYSTEM_SDK)
    include(CTest)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (LEXXOS_PLATFORM STREQUAL "pico" AND NOT TARGET pico_stdlib)
    pico_sdk_init()
endif ()

if ((LEXXOS_PLATFORM STREQUAL "pico" AND LEXXOS_PICO_USE_FREERTOS) OR (LEXXOS_PLATFORM STREQUAL "stm32f4" AND NOT LEXXOS_STM32F4_BRINGUP_ONLY))
    add_library(FreeRTOS-Config INTERFACE)
    target_include_directories(FreeRTOS-Config SYSTEM INTERFACE
            ${LEXXOS_ROOT_DIR}/config
    )

    if (LEXXOS_PLATFORM STREQUAL "pico")
        target_compile_definitions(FreeRTOS-Config INTERFACE
                LEXXOS_PLATFORM_PICO=1
                LEXXOS_PICO_FREERTOS_CORES=${LEXXOS_PICO_FREERTOS_CORES}
                LEXXOS_SD_IO_WORKER_CORE=${LEXXOS_SD_IO_WORKER_CORE}
        )
    elseif (LEXXOS_PLATFORM STREQUAL "stm32f4")
        target_compile_definitions(FreeRTOS-Config INTERFACE LEXXOS_PLATFORM_STM32F4=1)
    endif ()

    include("${LEXXOS_ROOT_DIR}/config/cmake/freertos_import.cmake")
endif ()

include("${LEXXOS_ROOT_DIR}/config/cmake/Features.cmake")
include("${LEXXOS_ROOT_DIR}/config/cmake/BuildInfo.cmake")
include("${LEXXOS_ROOT_DIR}/config/cmake/lexxos_target.cmake")
if (NOT LEXXOS_SYSTEM_SDK)
    include("${LEXXOS_ROOT_DIR}/config/cmake/lexxos_openocd.cmake")
endif ()

if (LEXXOS_FEATURE_FATFS)
    include("${LEXXOS_ROOT_DIR}/config/cmake/FatFs.cmake")
    if (LEXXOS_SYSTEM_SDK AND LEXXOS_PLATFORM MATCHES "^(host|sim)$")
        set_target_properties(fatfs PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif ()
endif ()

add_subdirectory("${LEXXOS_ROOT_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/src")

if (NOT LEXXOS_SYSTEM_SDK AND BUILD_TESTING AND LEXXOS_PLATFORM STREQUAL "host")
    add_subdirectory("${LEXXOS_ROOT_DIR}/tests" "${CMAKE_BINARY_DIR}/tests")
endif ()

if (NOT LEXXOS_SYSTEM_SDK AND TARGET lexxos)
    lexxos_attach_local_openocd_dependency(lexxos)
endif ()
