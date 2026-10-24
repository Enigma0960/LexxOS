# Minimal FreeRTOS kernel importer for LexxOS embedded targets.
#
# Set FREERTOS_KERNEL_PATH to a FreeRTOS-Kernel checkout, for example:
#   cmake -S . -B build -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
# or export FREERTOS_KERNEL_PATH before configuring the project.

include("${CMAKE_CURRENT_LIST_DIR}/freertos_rp2350_smp_port.cmake")

if (DEFINED ENV{FREERTOS_KERNEL_PATH} AND NOT FREERTOS_KERNEL_PATH)
    set(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}")
endif ()

if (NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "FREERTOS_KERNEL_PATH is not set. Clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git and pass -DFREERTOS_KERNEL_PATH=<path>.")
endif ()

get_filename_component(FREERTOS_KERNEL_PATH "${FREERTOS_KERNEL_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
set(FREERTOS_KERNEL_PATH "${FREERTOS_KERNEL_PATH}" CACHE PATH "Path to the FreeRTOS-Kernel checkout" FORCE)
if (NOT DEFINED LEXXOS_FREERTOS_SOURCE_KERNEL_PATH)
    set(LEXXOS_FREERTOS_SOURCE_KERNEL_PATH "${FREERTOS_KERNEL_PATH}" CACHE PATH "Immutable source FreeRTOS-Kernel checkout for generated SMP builds")
endif ()
set(_lexxos_freertos_source_kernel_path "${LEXXOS_FREERTOS_SOURCE_KERNEL_PATH}")

if (NOT EXISTS "${FREERTOS_KERNEL_PATH}")
    message(FATAL_ERROR "FREERTOS_KERNEL_PATH does not exist: ${FREERTOS_KERNEL_PATH}")
endif ()

if (LEXXOS_PLATFORM STREQUAL "pico")
    if (PICO_PLATFORM STREQUAL "rp2040")
        set(_lexxos_freertos_port_root "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC")
        if (NOT EXISTS "${_lexxos_freertos_port_root}")
            set(_lexxos_freertos_port_root "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC")
        endif ()

        set(_lexxos_freertos_port "RP2040")

        set(_lexxos_freertos_port_dir "${_lexxos_freertos_port_root}/${_lexxos_freertos_port}")
        if (NOT EXISTS "${_lexxos_freertos_port_dir}/CMakeLists.txt")
            message(FATAL_ERROR "FreeRTOS Pico port was not found: ${_lexxos_freertos_port_dir}")
        endif ()
        message(STATUS "Using FreeRTOS kernel: ${FREERTOS_KERNEL_PATH}")
        message(STATUS "Using FreeRTOS Pico port: ${_lexxos_freertos_port}")


        add_subdirectory("${_lexxos_freertos_port_dir}" "${CMAKE_BINARY_DIR}/freertos")
    elseif (PICO_PLATFORM STREQUAL "rp2350-arm-s")
        set(_lexxos_pico_freertos_cores 1)
        if (DEFINED LEXXOS_PICO_FREERTOS_CORES)
            set(_lexxos_pico_freertos_cores "${LEXXOS_PICO_FREERTOS_CORES}")
        endif ()

        if (_lexxos_pico_freertos_cores GREATER 1 AND LEXXOS_EXPERIMENTAL_RP2350_SMP_FREERTOS)
            set(_lexxos_freertos_port "RP2350_ARM_NTZ")
            set(_lexxos_freertos_source_port_dir "${_lexxos_freertos_source_kernel_path}/portable/ThirdParty/GCC/${_lexxos_freertos_port}")
            if (NOT EXISTS "${_lexxos_freertos_source_port_dir}/CMakeLists.txt")
                set(_lexxos_freertos_source_port_dir "${_lexxos_freertos_source_kernel_path}/portable/ThirdParty/Community-Supported-Ports/GCC/${_lexxos_freertos_port}")
            endif ()

            set(_lexxos_freertos_generated_kernel_dir "${CMAKE_BINARY_DIR}/generated/freertos-rp2350-smp-kernel")
            set(_lexxos_freertos_port_dir "${CMAKE_BINARY_DIR}/generated/freertos-rp2350-smp-port")
            lexxos_prepare_rp2350_smp_freertos_port(
                "${_lexxos_freertos_source_kernel_path}"
                "${_lexxos_freertos_source_port_dir}"
                "${_lexxos_freertos_generated_kernel_dir}"
                "${_lexxos_freertos_port_dir}")
            set(FREERTOS_KERNEL_PATH "${_lexxos_freertos_generated_kernel_dir}" CACHE PATH "Path to the FreeRTOS-Kernel checkout" FORCE)

            message(STATUS "Using FreeRTOS kernel: ${FREERTOS_KERNEL_PATH}")
            message(STATUS "Using FreeRTOS Pico port: ${_lexxos_freertos_port} SMP (${_lexxos_pico_freertos_cores} cores)")
            message(STATUS "Using patched FreeRTOS RP2350 SMP port: ${_lexxos_freertos_port_dir}")

            add_subdirectory("${_lexxos_freertos_port_dir}" "${CMAKE_BINARY_DIR}/freertos-rp2350-smp")
        else ()
            if (_lexxos_pico_freertos_cores GREATER 1)
                message(STATUS "Using stable single-core FreeRTOS RP2350 port; set LEXXOS_EXPERIMENTAL_RP2350_SMP_FREERTOS=ON to opt into SMP")
            endif ()
            set(_lexxos_freertos_port "GCC/ARM_CM33_NTZ/non_secure")
            set(_lexxos_freertos_port_dir "${FREERTOS_KERNEL_PATH}/portable/${_lexxos_freertos_port}")
            set(_lexxos_freertos_heap4 "${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c")
            set(_lexxos_pico_freertos_sdk_config "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include/freertos_sdk_config.h")

            foreach (_lexxos_required_freertos_file IN ITEMS
                    "${_lexxos_freertos_port_dir}/port.c"
                    "${_lexxos_freertos_port_dir}/portasm.c"
                    "${_lexxos_freertos_port_dir}/mpu_wrappers_v2_asm.c"
                    "${_lexxos_freertos_port_dir}/portmacro.h"
                    "${_lexxos_freertos_heap4}"
                    "${_lexxos_pico_freertos_sdk_config}")
                if (NOT EXISTS "${_lexxos_required_freertos_file}")
                    message(FATAL_ERROR "Required FreeRTOS RP2350 file was not found: ${_lexxos_required_freertos_file}")
                endif ()
            endforeach ()

            message(STATUS "Using FreeRTOS kernel: ${FREERTOS_KERNEL_PATH}")
            message(STATUS "Using FreeRTOS Pico port: ${_lexxos_freertos_port}")

            add_library(FreeRTOS-Kernel STATIC
                    "${FREERTOS_KERNEL_PATH}/croutine.c"
                    "${FREERTOS_KERNEL_PATH}/event_groups.c"
                    "${FREERTOS_KERNEL_PATH}/list.c"
                    "${FREERTOS_KERNEL_PATH}/queue.c"
                    "${FREERTOS_KERNEL_PATH}/stream_buffer.c"
                    "${FREERTOS_KERNEL_PATH}/tasks.c"
                    "${FREERTOS_KERNEL_PATH}/timers.c"
                    "${_lexxos_freertos_port_dir}/port.c"
                    "${_lexxos_freertos_port_dir}/portasm.c"
                    "${_lexxos_freertos_port_dir}/mpu_wrappers_v2_asm.c"
            )
            target_include_directories(FreeRTOS-Kernel SYSTEM PUBLIC
                    "${FREERTOS_KERNEL_PATH}/include"
                    "${_lexxos_freertos_port_dir}"
            )
            target_link_libraries(FreeRTOS-Kernel PUBLIC
                    FreeRTOS-Config
                    pico_base_headers
            )
            target_compile_definitions(FreeRTOS-Kernel PUBLIC
                    LIB_FREERTOS_KERNEL=1
                    PICO_RP2350=1
                    PICO_CONFIG_RTOS_ADAPTER_HEADER=${_lexxos_pico_freertos_sdk_config}
            )
            target_compile_options(FreeRTOS-Kernel PRIVATE
                    -Wall
                    -Wextra
                    -Wno-unused-parameter
            )

            add_library(FreeRTOS-Kernel-Heap4 STATIC "${_lexxos_freertos_heap4}")
            target_include_directories(FreeRTOS-Kernel-Heap4 SYSTEM PUBLIC
                    "${FREERTOS_KERNEL_PATH}/include"
                    "${_lexxos_freertos_port_dir}"
            )
            target_link_libraries(FreeRTOS-Kernel-Heap4 PUBLIC FreeRTOS-Config)
            target_compile_definitions(FreeRTOS-Kernel-Heap4 PUBLIC
                    LIB_FREERTOS_KERNEL=1
                    PICO_RP2350=1
            )
        endif ()
    else ()
        message(FATAL_ERROR "FreeRTOS Pico support is not integrated for PICO_PLATFORM='${PICO_PLATFORM}'. Supported Pico FreeRTOS platforms: rp2040, rp2350-arm-s.")
    endif ()
elseif (LEXXOS_PLATFORM STREQUAL "stm32f4")
    set(_lexxos_freertos_port_dir "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM4F")
    set(_lexxos_freertos_heap4 "${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c")

    if (NOT EXISTS "${_lexxos_freertos_port_dir}/port.c")
        message(FATAL_ERROR "FreeRTOS Cortex-M4F port was not found: ${_lexxos_freertos_port_dir}")
    endif ()
    if (NOT EXISTS "${_lexxos_freertos_heap4}")
        message(FATAL_ERROR "FreeRTOS heap_4.c was not found: ${_lexxos_freertos_heap4}")
    endif ()

    message(STATUS "Using FreeRTOS kernel: ${FREERTOS_KERNEL_PATH}")
    message(STATUS "Using FreeRTOS STM32F4 port: ${_lexxos_freertos_port_dir}")

    add_library(FreeRTOS-Kernel STATIC
            "${FREERTOS_KERNEL_PATH}/croutine.c"
            "${FREERTOS_KERNEL_PATH}/event_groups.c"
            "${FREERTOS_KERNEL_PATH}/list.c"
            "${FREERTOS_KERNEL_PATH}/queue.c"
            "${FREERTOS_KERNEL_PATH}/stream_buffer.c"
            "${FREERTOS_KERNEL_PATH}/tasks.c"
            "${FREERTOS_KERNEL_PATH}/timers.c"
            "${_lexxos_freertos_port_dir}/port.c"
    )
    target_include_directories(FreeRTOS-Kernel SYSTEM PUBLIC
            "${FREERTOS_KERNEL_PATH}/include"
            "${_lexxos_freertos_port_dir}"
    )
    target_link_libraries(FreeRTOS-Kernel PUBLIC FreeRTOS-Config)
    target_compile_options(FreeRTOS-Kernel PRIVATE
            -mcpu=cortex-m4
            -mthumb
            -mfpu=fpv4-sp-d16
            -mfloat-abi=hard
            -ffunction-sections
            -fdata-sections
            -Wall
            -Wextra
            -Wno-unused-parameter
    )

    add_library(FreeRTOS-Kernel-Heap4 STATIC "${_lexxos_freertos_heap4}")
    target_include_directories(FreeRTOS-Kernel-Heap4 SYSTEM PUBLIC
            "${FREERTOS_KERNEL_PATH}/include"
            "${_lexxos_freertos_port_dir}"
    )
    target_link_libraries(FreeRTOS-Kernel-Heap4 PUBLIC FreeRTOS-Config)
    target_compile_options(FreeRTOS-Kernel-Heap4 PRIVATE
            -mcpu=cortex-m4
            -mthumb
            -mfpu=fpv4-sp-d16
            -mfloat-abi=hard
            -ffunction-sections
            -fdata-sections
            -Wall
            -Wextra
            -Wno-unused-parameter
    )
endif ()
