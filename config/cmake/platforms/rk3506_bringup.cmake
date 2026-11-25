# RK3506 использует собственный загрузочный тракт и linker script.
# Shell-профиль подключает общий runtime; bootstrap проверяет только платформу.
include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/Version.cmake")
project(LexxOSRK3506Bringup VERSION ${LEXXOS_VERSION} LANGUAGES C CXX ASM)

set(_rk3506_source "${CMAKE_CURRENT_LIST_DIR}/../../../src/lexxos/platform/rockchip/rk3506")
get_filename_component(_rk3506_source "${_rk3506_source}" ABSOLUTE)
if (LEXXOS_SYSTEM_SDK)
    if (NOT CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Select the RK3506 Arm toolchain before project() in your project")
    endif ()
    if (DEFINED LEXXOS_RK3506_SHELL AND NOT LEXXOS_RK3506_SHELL)
        message(FATAL_ERROR "LexxSDK on RK3506 requires LEXXOS_RK3506_SHELL=ON")
    endif ()
else ()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif ()

# Stage 6: the *existing* LexxOS app::Runtime and Shell on UART0.
# OFF restores the hardware-proven stage-5 scheduler/heartbeat diagnostics.
option(LEXXOS_RK3506_SHELL "Boot the real LexxOS Runtime and Shell over UART0" ON)

set(LEXXOS_RK3506_SCHEDULER "cooperative" CACHE STRING
        "RK3506 scheduler: cooperative or freertos (experimental bootstrap)")
set_property(CACHE LEXXOS_RK3506_SCHEDULER PROPERTY STRINGS cooperative freertos)
if (NOT LEXXOS_RK3506_SCHEDULER STREQUAL "cooperative"
        AND NOT LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
    message(FATAL_ERROR "LEXXOS_RK3506_SCHEDULER must be cooperative or freertos")
endif ()

# First integration of the real Runtime/Shell: UART and core programs only.
# Hardware drivers and concurrent board tasks will be enabled separately.
set(_rk3506_freertos_console_only OFF)
if (LEXXOS_RK3506_SCHEDULER STREQUAL "freertos" AND LEXXOS_RK3506_SHELL)
    set(_rk3506_freertos_console_only ON)
endif ()

option(LEXXOS_RK3506_FREERTOS_PERIPHERALS
        "FreeRTOS: enable PicoCalc SD, LCD and keyboard peripherals"
        OFF)
if (LEXXOS_RK3506_FREERTOS_PERIPHERALS AND NOT _rk3506_freertos_console_only)
    message(FATAL_ERROR "FreeRTOS peripherals require SCHEDULER=freertos and SHELL=ON")
endif ()

# The system boot SDMMC (sd0) is never writable here. Removable PicoCalc
# SPI1 sd1 follows the board default (FAT RW) without an opt-in build flag.
# A recovery image can override only sd1 to read-only.
option(LEXXOS_RK3506_SD_FORCE_READ_ONLY
        "Recovery: force removable PicoCalc SPI1 sd1 to FAT read-only" OFF)
# Build a bounded CLMT only for large .lexx package reads. Default ON for Lyra.
option(LEXXOS_FATFS_FASTSEEK "Enable optional FatFs cluster-map acceleration on RK3506" ON)
if (DEFINED LEXXOS_RK3506_SD_WRITE_EXPERIMENTAL
        OR DEFINED LEXXOS_RK3506_SD_MOUNT_RW_AT_BOOT)
    message(STATUS "RK3506: obsolete SD_WRITE_EXPERIMENTAL/SD_MOUNT_RW_AT_BOOT options ignored; sd1 now follows the board RW policy")
endif ()

if (LEXXOS_RK3506_SCHEDULER STREQUAL "freertos" AND LEXXOS_RK3506_MANAGED_TIMER_IRQ)
    message(FATAL_ERROR "RK3506 FreeRTOS managed IRQ integration is pending: set MANAGED_TIMER_IRQ=OFF")
endif ()

# Opt-in KRN-1: route CPU0 Generic Timer PPI 27 through the common IrqManager.
# OFF preserves the existing hardware-tested IRQ path without any changes.
option(LEXXOS_RK3506_MANAGED_TIMER_IRQ "Use the KRN-1 IRQ manager for PPI 27" OFF)

# Experimental: DMAC0 event INTID 148 (DTB GIC_SPI 116); second INTID 149 stays unclaimed.
option(LEXXOS_RK3506_MANAGED_DMA_IRQ "Use KRN-1 IRQ manager for DMAC0 IRQ148 (requires managed timer)" OFF)
if (LEXXOS_RK3506_MANAGED_DMA_IRQ AND NOT LEXXOS_RK3506_MANAGED_TIMER_IRQ)
    message(FATAL_ERROR "LEXXOS_RK3506_MANAGED_DMA_IRQ requires LEXXOS_RK3506_MANAGED_TIMER_IRQ=ON")
endif ()

option(LEXXOS_RK3506_SYSTEM_BENCHMARK "Include portable bench in the FreeRTOS shell without raw diagnostics" ON)

# Keep raw SPI/PL330 diagnostics out of normal production firmware.
option(LEXXOS_RK3506_SD_DIAGNOSTICS "Include RK3506 SD low-level diagnostics and benchmark" OFF)

# Explicit opt-in: PMU access is protected against undefined instructions in
# Non-secure PL1. PMU is claimed only for the SD stress benchmark on CPU0.
option(LEXXOS_RK3506_PMU_PROFILING "Profile Cortex-A7 instructions/cycles in RK3506 SD stress" OFF)

# SWD works with an MMU in hardware, but the current GDB RAM reload monitor
# requires the CPU to use physical addresses with MMU and D-cache disabled.
# Keep this switch ON by default to preserve the proven debug workflow.
option(LEXXOS_RK3506_SWD_DEBUG
        "Enable RK3506 SWD handoff and existing GDB DDR reload workflow" ON)

# UART0 remains the unconditional boot console. The runtime command
# `debug` initializes UART1 on demand and enables SWD explicitly.
# Legacy CMake flags must never affect startup (old caches may still contain them).
if (DEFINED LEXXOS_RK3506_CONSOLE OR DEFINED LEXXOS_RK3506_ENABLE_SWD)
    message(STATUS "RK3506: LEXXOS_RK3506_CONSOLE/ENABLE_SWD are obsolete; use 'debug' at runtime")
endif ()

# Заголовки board/feature нужны адаптеру платформы даже без shell.
include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/platforms/rk3506_shell_profile.cmake")
if (NOT LEXXOS_RK3506_SHELL)
    set(LEXXOS_FEATURE_DISPLAY OFF)
    set(LEXXOS_FEATURE_DISPLAY_TEST_TRACE OFF)
endif ()

if (_rk3506_freertos_console_only)
    # Keep hardware diagnostics disabled for both profiles.
    # SD and LCD DMA are configured independently of this feature selection.
    set(LEXXOS_FEATURE_DEBUG_DISPLAY OFF)
    set(LEXXOS_FEATURE_DISPLAY_TEST_COMMANDS OFF)
    set(LEXXOS_FEATURE_DISPLAY_TEST_TRACE OFF)
    set(LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS OFF)
    set(LEXXOS_FEATURE_KEYBOARD_TEST_COMMANDS OFF)
    set(LEXXOS_FEATURE_DS3231_RTC OFF)
    set(LEXXOS_FEATURE_GPIO_ANALYZER OFF)
    set(LEXXOS_FEATURE_TEST_COMMANDS OFF)
    set(LEXXOS_COMMANDS_TESTS OFF)
    set(LEXXOS_COMMAND_BENCH ${LEXXOS_RK3506_SYSTEM_BENCHMARK})

    if (LEXXOS_RK3506_FREERTOS_PERIPHERALS)
        set(LEXXOS_FEATURE_SD_CARD ON)
        set(LEXXOS_FEATURE_FATFS ON)
        set(LEXXOS_FEATURE_KEYBOARD ON)
        message(STATUS "RK3506 FreeRTOS: UART + PicoCalc SD FAT + LCD + keyboard")
    else ()
        set(LEXXOS_FEATURE_DISPLAY OFF)
        set(LEXXOS_FEATURE_SD_CARD OFF)
        set(LEXXOS_FEATURE_KEYBOARD OFF)
        message(STATUS "RK3506 FreeRTOS: UART-only LexxOS Runtime/Shell")
    endif ()
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/Features.cmake")

if (LEXXOS_RK3506_SHELL)
    include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/BuildInfo.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/lexxos_sources.cmake")
endif ()

if (LEXXOS_SYSTEM_SDK)
    add_library(lexxos STATIC)
else ()
    add_executable(lexxos)
endif ()
target_sources(lexxos PRIVATE
        "${_rk3506_source}/startup.S"
        "${_rk3506_source}/exception_vectors.S"
        "${_rk3506_source}/early_uart.cpp"
        "${_rk3506_source}/debug_uart1.cpp"
        "${_rk3506_source}/irq_timer.cpp"
        "${_rk3506_source}/dma_irq_clients.cpp"
        "${_rk3506_source}/dmac0_controller.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_debug_port.cpp"
        "${_rk3506_source}/cpp_runtime.cpp"
        "${_rk3506_source}/platform_rk3506.cpp"
        "${_rk3506_source}/gpio_output.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_platform_bindings.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_dma_pl330.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/sd_dma_controller.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_dma.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/platform/platform_services.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/scheduler_backend.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/platform/processor.cpp"
)

if (_rk3506_freertos_console_only)
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_FREERTOS_CONSOLE_ONLY=1)


    if (LEXXOS_RK3506_FREERTOS_PERIPHERALS)
        target_compile_definitions(lexxos PRIVATE
                LEXXOS_RK3506_FREERTOS_PERIPHERALS=1)
    endif ()
endif ()

if (LEXXOS_RK3506_SD_FORCE_READ_ONLY)
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_SD_FORCE_READ_ONLY=1)
    message(STATUS "RK3506: removable SPI1 sd1 forced read-only for recovery")
endif ()

# Independent FreeRTOS heap_4 budget. Tasks, queues and .lexx guest arenas
# share this heap; the CPU0 BOOTRAM linker region bounds the final image.
set(LEXXOS_RK3506_FREERTOS_HEAP_KIB "512" CACHE STRING
        "RK3506 FreeRTOS heap_4 size in KiB (128..2048)")
if (NOT LEXXOS_RK3506_FREERTOS_HEAP_KIB MATCHES "^[0-9]+$")
    message(FATAL_ERROR "LEXXOS_RK3506_FREERTOS_HEAP_KIB must be an integer")
endif ()
if (LEXXOS_RK3506_FREERTOS_HEAP_KIB LESS 128
        OR LEXXOS_RK3506_FREERTOS_HEAP_KIB GREATER 2048)
    message(FATAL_ERROR "LEXXOS_RK3506_FREERTOS_HEAP_KIB must be between 128 and 2048")
endif ()

if (LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
    math(EXPR _rk3506_freertos_heap_bytes "${LEXXOS_RK3506_FREERTOS_HEAP_KIB} * 1024")
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_FREERTOS_HEAP_BYTES=${_rk3506_freertos_heap_bytes})
    message(STATUS "RK3506 FreeRTOS heap_4: ${LEXXOS_RK3506_FREERTOS_HEAP_KIB} KiB")

    set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS" CACHE PATH
            "FreeRTOS-Kernel source (defaults to the pinned submodule)")
    set(_rk3506_freertos_port "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CA9")

    set(_rk3506_freertos_compat_port
            "${CMAKE_CURRENT_BINARY_DIR}/generated/freertos-rk3506")

    # The pinned ARM_CA9 port uses legacy FMXR/FMRX mnemonics, which
    # modern ARM assemblers reject when targeting Cortex-A7.
    # Adapt copies in the build directory; never modify the submodule.
    foreach (_port_file IN ITEMS port.c portASM.S)
        if (NOT EXISTS "${_rk3506_freertos_port}/${_port_file}")
            message(FATAL_ERROR
                    "FreeRTOS ARM_CA9 port file missing: ${_port_file}")
        endif ()
    endforeach ()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${_rk3506_freertos_port}/port.c"
            "${_rk3506_freertos_port}/portASM.S")

    file(READ "${_rk3506_freertos_port}/port.c" _rk3506_ca9_port_c)
    file(READ "${_rk3506_freertos_port}/portASM.S" _rk3506_ca9_port_asm)
    string(FIND "${_rk3506_ca9_port_c}" "FMXR  FPSCR" _rk3506_fmxr_position)
    string(FIND "${_rk3506_ca9_port_asm}" "FMRX" _rk3506_fmrx_position)

    if (_rk3506_fmxr_position EQUAL -1 OR _rk3506_fmrx_position EQUAL -1)
        message(FATAL_ERROR
                "FreeRTOS ARM_CA9 port has changed; review the Cortex-A7 VFP adaptation")
    endif ()

    string(REPLACE "FMXR  FPSCR" "VMSR  FPSCR"
            _rk3506_ca9_port_c "${_rk3506_ca9_port_c}")
    string(REPLACE "FMRX" "VMRS"
            _rk3506_ca9_port_asm "${_rk3506_ca9_port_asm}")

    # Keep ARM_CA9 port assertions (including FromISR priority validation).
    # Only skip the upstream priority-probe against firmware-owned GIC SPI32.
    set(_rk3506_foreign_irq_probe
            "    #if ( configASSERT_DEFINED == 1 )\n    {\n        volatile uint8_t ucOriginalPriority;")
    string(FIND "${_rk3506_ca9_port_c}" "${_rk3506_foreign_irq_probe}" _rk3506_foreign_irq_probe_at)
    if (_rk3506_foreign_irq_probe_at EQUAL -1)
        message(FATAL_ERROR
                "FreeRTOS ARM_CA9 GIC startup probe changed; review RK3506 assertion adaptation")
    endif ()
    string(REPLACE "${_rk3506_foreign_irq_probe}"
            "    #if 0 /* RK3506: do not probe firmware-owned SPI32. */\n    {\n        volatile uint8_t ucOriginalPriority;"
            _rk3506_ca9_port_c "${_rk3506_ca9_port_c}")
    file(MAKE_DIRECTORY "${_rk3506_freertos_compat_port}")
    file(WRITE "${_rk3506_freertos_compat_port}/port.c" "${_rk3506_ca9_port_c}")
    file(WRITE "${_rk3506_freertos_compat_port}/portASM.S" "${_rk3506_ca9_port_asm}")


    set(_rk3506_freertos_sources
            "${FREERTOS_KERNEL_PATH}/tasks.c"
            "${FREERTOS_KERNEL_PATH}/list.c"
            "${FREERTOS_KERNEL_PATH}/queue.c"
            "${FREERTOS_KERNEL_PATH}/event_groups.c"
            "${FREERTOS_KERNEL_PATH}/stream_buffer.c"
            "${FREERTOS_KERNEL_PATH}/timers.c"
            "${FREERTOS_KERNEL_PATH}/croutine.c"
            "${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c"
            "${_rk3506_freertos_compat_port}/port.c"
            "${_rk3506_freertos_compat_port}/portASM.S"
    )
    foreach (_source IN LISTS _rk3506_freertos_sources)
        if (NOT EXISTS "${_source}")
            message(FATAL_ERROR "FreeRTOS source missing: ${_source}. Run git submodule update --init third_party/FreeRTOS")
        endif ()
    endforeach ()
    target_sources(lexxos PRIVATE
            ${_rk3506_freertos_sources}
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/scheduler_freertos.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/hooks.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/synchronization_freertos.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/event_freertos.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/mutex_freertos.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/freertos/semaphore_freertos.cpp"
            "${_rk3506_source}/freertos_port.cpp")
    target_include_directories(lexxos SYSTEM PRIVATE
            "${FREERTOS_KERNEL_PATH}/include"
            "${_rk3506_freertos_port}")
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_PLATFORM_RK3506=1
            LEXXOS_RK3506_FREERTOS=1
            LEXXOS_KERNEL_FREERTOS=1)

    # ARM_CA9 uses VFP instructions for optional context management.
    # softfp permits VFP instructions while preserving the base AAPCS
    # calling convention used by the rest of LexxOS (-mfloat-abi=soft).
    # Apply only to the FreeRTOS port, not the entire firmware.
    set_source_files_properties(
            "${_rk3506_freertos_compat_port}/port.c"
            "${_rk3506_freertos_compat_port}/portASM.S"
            PROPERTIES COMPILE_OPTIONS "-mfpu=neon-vfpv4;-mfloat-abi=softfp")

    #    # Upstream ARM_CA9 assertion setup writes to SPI32, which we do not own.
    #    # Disable this port.c-only probe while retaining kernel assertions.
    #    set_source_files_properties("${_rk3506_freertos_compat_port}/port.c"
    #            PROPERTIES COMPILE_DEFINITIONS "LEXXOS_RK3506_FREERTOS_PORT_C=1")
    target_link_libraries(lexxos PRIVATE gcc)
else ()
    target_sources(lexxos PRIVATE
            "${_rk3506_source}/task_switch.S"
            "${_rk3506_source}/cooperative_tasks.cpp")
endif ()

if (LEXXOS_RK3506_MANAGED_TIMER_IRQ)
    target_sources(lexxos PRIVATE
            "${_rk3506_source}/gic_irq_controller.cpp")
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_MANAGED_TIMER_IRQ=1)
endif ()

if (LEXXOS_RK3506_MANAGED_DMA_IRQ)
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_MANAGED_DMA_IRQ=1)
endif ()

if (LEXXOS_RK3506_PMU_PROFILING)
    target_sources(lexxos PRIVATE
            "${_rk3506_source}/pmu_probe.S"
            "${_rk3506_source}/pmu_profile.cpp")
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_PMU_PROFILING=1)
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/MemoryProfiles.cmake")
lexxos_attach_memory_profile(lexxos CHIP rk3506g2 PLATFORM rk3506 BOARD picocalc_lyra)

if (NOT LEXXOS_SYSTEM_SDK)
    set_target_properties(lexxos PROPERTIES SUFFIX ".elf" LINK_DEPENDS "${_rk3506_source}/linker.ld")
endif ()

if (LEXXOS_RK3506_SHELL)
    # A single source-of-truth catalog: these are the same Shell/Runtime,
    # ProgramManager, VFS and program translation units used by other boards.
    # platform_services/processor/scheduler_backend already live in the
    # bootstrap target. Avoid relying on cross-platform driver fallbacks.
    list(REMOVE_ITEM LEXXOS_COMMON_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/platform/platform_services.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/platform/processor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/kernel/scheduler_backend.cpp")
    target_sources(lexxos PRIVATE ${LEXXOS_COMMON_SOURCES}
            "${_rk3506_source}/debug_program.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_i2c_bus.cpp"
            "${_rk3506_source}/i2c2_io.cpp"
            "${_rk3506_source}/syscalls.cpp"
            "${_rk3506_source}/sdmmc_probe.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/apps/storage/rk3506/sdmmc_program.cpp"
            "${_rk3506_source}/sdmmc_block_device.cpp"
            "${_rk3506_source}/sdmmc_fat_ro.cpp"
            "${_rk3506_source}/sdmmc_ext_inspect.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_transport.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/sd_protocol.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/native_sd_protocol_io.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_protocol.cpp"
            "${_rk3506_source}/sd_gpio.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_storage.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_mount_policy.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_block_device.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_spi1_hw.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/sd_spi_controller.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_spi.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs/source/ff.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs/source/ffunicode.c"
    )
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_SHELL=1
            LEXXOS_PLATFORM_RK3506=1
            LEXXOS_BOARD_NAME=\"${LEXXOS_BOARD}\")

    set(LEXXOS_RK3506_I2C_BACKEND "hardware" CACHE STRING
            "PicoCalc Lyra I2C backend: hardware or software")
    set_property(CACHE LEXXOS_RK3506_I2C_BACKEND PROPERTY STRINGS hardware software)
    if (LEXXOS_RK3506_I2C_BACKEND STREQUAL "hardware")
        target_sources(lexxos PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/rk3506_i2c_bus.cpp")
    elseif (LEXXOS_RK3506_I2C_BACKEND STREQUAL "software")
        target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_I2C_SOFTWARE=1)
    else ()
        message(FATAL_ERROR "LEXXOS_RK3506_I2C_BACKEND must be hardware or software")
    endif ()

    # The PicoCalc Lyra display's validated default is RGB565 via SPI0 DFS16
    # and PL330 at 46.875 MHz. Other RK3506 products keep conservative defaults.
    # Explicit -D overrides and existing CMake cache values take precedence.
    if (LEXXOS_BOARD_PICOCALC_LYRA)
        set(_rk3506_lcd_default_hz "46875000")
        set(_rk3506_lcd_default_wire "on")
        set(_rk3506_lcd_default_direct16 ON)
    else ()
        set(_rk3506_lcd_default_hz "23437500")
        set(_rk3506_lcd_default_wire "auto")
        set(_rk3506_lcd_default_direct16 OFF)
    endif ()

    # SPI0 LCD only; SPI1 SD keeps its own clock and transfer policy.
    set(LEXXOS_RK3506_LCD_SPI_HZ "${_rk3506_lcd_default_hz}" CACHE STRING
            "RK3506 LCD SPI0 frequency in Hz")
    set_property(CACHE LEXXOS_RK3506_LCD_SPI_HZ PROPERTY STRINGS
            6000000 12000000 23437500 31250000 46875000)
    if (NOT LEXXOS_RK3506_LCD_SPI_HZ MATCHES
            "^(6000000|12000000|23437500|31250000|46875000)$")
        message(FATAL_ERROR "Unsupported RK3506 LCD SPI0 frequency")
    endif ()

    # RGB565 is hardware-validated and now the default for PicoCalc Lyra.
    # Other RK3506 products may keep the RGB666 default.
    set(LEXXOS_ILI9488_RGB565_WIRE "${_rk3506_lcd_default_wire}" CACHE STRING
            "ILI9488 SPI wire format: auto/off=RGB666, on=RGB565")
    set_property(CACHE LEXXOS_ILI9488_RGB565_WIRE PROPERTY STRINGS
            auto on off)
    string(TOLOWER "${LEXXOS_ILI9488_RGB565_WIRE}"
            _rk3506_ili9488_wire_mode)
    if (NOT _rk3506_ili9488_wire_mode STREQUAL "auto"
            AND NOT _rk3506_ili9488_wire_mode STREQUAL "on"
            AND NOT _rk3506_ili9488_wire_mode STREQUAL "off")
        message(FATAL_ERROR
                "LEXXOS_ILI9488_RGB565_WIRE must be auto, on or off")
    endif ()

    option(LEXXOS_RK3506_LCD_DIRECT16_DEFAULT
            "Enable validated SPI0 DFS16 + PL330 RGB565 path by default" ${_rk3506_lcd_default_direct16})

    if (LEXXOS_FEATURE_DISPLAY)
        if (LEXXOS_RK3506_LCD_DIRECT16_DEFAULT)
            if (NOT _rk3506_ili9488_wire_mode STREQUAL "on")
                message(FATAL_ERROR "Default LCD direct16 requires LEXXOS_ILI9488_RGB565_WIRE=on")
            endif ()
            target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_LCD_DIRECT16_DEFAULT=1)
        endif ()

        if (_rk3506_ili9488_wire_mode STREQUAL "on")
            target_compile_definitions(lexxos PRIVATE
                    LEXXOS_ILI9488_RGB565_WIRE=1)
        endif ()

        target_compile_definitions(lexxos PRIVATE
                LEXXOS_RK3506_LCD_SPI_HZ=${LEXXOS_RK3506_LCD_SPI_HZ})
        target_sources(lexxos PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_spi0.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_spi_controller.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_lcd_spi.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_lcd_dma.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_dma_pl330.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_dma_controller.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_tx_program.cpp"
        )
    endif ()

    if (LEXXOS_RK3506_SD_DIAGNOSTICS)
        target_sources(lexxos PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/apps/storage/rk3506/picosd_program.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/apps/storage/rk3506/sd_hardware_bench_program.cpp"
        )
        target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_SD_DIAGNOSTICS=1)
    endif ()

    target_include_directories(lexxos PRIVATE
            "${LEXXOS_GENERATED_INCLUDE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs/source")

    # Two independent FAT mount slots: removable SPI1 SD and optional
    # explicitly mounted system SDMMC partition (which always stays RO).
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_FAT_READ_CACHE_SECTORS=1
            LEXXOS_FATFS_DISKIO_BOUNCE_READS=0
            LEXXOS_FAT_MOUNT_SLOTS=2)

    # The RK3506 target compiles the upstream ff.c directly, not via fatfs.
    # Apply the same option to C, C++ and the FatVolume FIL layout.
    include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/FatFsConfigOverride.cmake")
    lexxos_apply_fatfs_config(lexxos PRIVATE)

    # The embedded C++ libraries and newlib are needed by existing LexxOS
    # application code. None of these links or starts a Linux kernel.
    target_link_libraries(lexxos PRIVATE
            "-Wl,--start-group" stdc++ m c gcc "-Wl,--end-group")
endif ()

option(LEXXOS_RK3506_ICACHE
        "Enable Cortex-A7 instruction cache without MMU/D-cache"
        ON)
if (LEXXOS_RK3506_ICACHE)
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_ICACHE=1)
endif ()

if (LEXXOS_RK3506_SWD_DEBUG)
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_SWD_DEBUG=1)
endif ()


# Experimental CPU0-only identity map for PicoCalc Lyra / RK3506.
# All DDR is Normal-NC by default; an optional CPU-private BOOTRAM region
# can use Write-Back/Write-Allocate when the FreeRTOS DMA-free path is selected.
option(LEXXOS_RK3506_MMU_NORMAL_NC
        "Experimental CPU0 MMU identity map: Normal-NC DDR, Device/XN MMIO" OFF)
option(LEXXOS_RK3506_DCACHE_BOOTRAM
        "Experimental D-cache on CPU0 BOOTRAM only (requires MMU + FreeRTOS PIO)" OFF)
if (LEXXOS_RK3506_DCACHE_BOOTRAM AND LEXXOS_RK3506_SWD_DEBUG)
    message(WARNING
            "RK3506 SWD has priority: disabling D-cache BOOTRAM experiment. "
            "Select LEXXOS_RK3506_SWD_DEBUG=OFF to test cached memory.")
    set(LEXXOS_RK3506_DCACHE_BOOTRAM OFF CACHE BOOL
            "Experimental D-cache on CPU0 BOOTRAM only (requires MMU + FreeRTOS PIO)" FORCE)
endif ()

if (LEXXOS_RK3506_MMU_NORMAL_NC AND LEXXOS_RK3506_SWD_DEBUG)
    message(WARNING
            "RK3506 MMU and SWD debug were both requested. SWD has priority: "
            "disabling LEXXOS_RK3506_MMU_NORMAL_NC because the current "
            "GDB DDR reload path requires MMU=0 and D-cache=0. "
            "For MMU experiments, explicitly set LEXXOS_RK3506_SWD_DEBUG=OFF.")
    set(LEXXOS_RK3506_MMU_NORMAL_NC OFF CACHE BOOL
            "Experimental CPU0 MMU identity map: Normal Non-cacheable DDR, no D-cache" FORCE)
endif ()

if (LEXXOS_RK3506_DCACHE_BOOTRAM)
    if (NOT LEXXOS_RK3506_MMU_NORMAL_NC)
        message(FATAL_ERROR "D-cache BOOTRAM requires LEXXOS_RK3506_MMU_NORMAL_NC=ON")
    endif ()
    if (NOT LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
        message(FATAL_ERROR "D-cache BOOTRAM is restricted to the FreeRTOS PIO profile")
    endif ()
    if (LEXXOS_RK3506_SD_DIAGNOSTICS)
        message(FATAL_ERROR "Disable RK3506 SD low-level DMA diagnostics for D-cache BOOTRAM")
    endif ()
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_DCACHE_BOOTRAM=1)
endif ()

# CPU0 managed VMSA: 4 KiB pages inside BOOTRAM, RX/RW-XN protection,
# non-cacheable DMA pool and fail-closed cache-maintenance primitives.
# Explicit opt-in until verified on the board; legacy modes remain available.
option(LEXXOS_RK3506_MMU_MANAGED "RK3506 4 KiB MMU/D-cache and uncached DMA pool" OFF)
if (LEXXOS_RK3506_MMU_MANAGED)
    if (LEXXOS_RK3506_SWD_DEBUG OR LEXXOS_RK3506_MMU_NORMAL_NC
            OR LEXXOS_RK3506_DCACHE_BOOTRAM)
        message(FATAL_ERROR
                "Managed MMU requires SWD_DEBUG=OFF, MMU_NORMAL_NC=OFF and DCACHE_BOOTRAM=OFF")
    endif ()
    if (NOT LEXXOS_BOARD_PICOCALC_LYRA
            OR NOT LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
        message(FATAL_ERROR "Managed MMU is currently CPU0 PicoCalc Lyra FreeRTOS only")
    endif ()
    if (LEXXOS_RK3506_SD_DIAGNOSTICS)
        message(FATAL_ERROR "Managed MMU requires SD_DIAGNOSTICS=OFF until DMA is audited")
    endif ()
    target_sources(lexxos PRIVATE
            "${_rk3506_source}/mmu_managed.cpp"
            "${_rk3506_source}/fdt_memory_map.cpp")
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_MMU_MANAGED=1)
    message(STATUS "RK3506 managed MMU: RX/WBWA kernel, RW-XN/WBWA data, uncached DMA pool")
endif ()

# Experimental CPU0 ownership of 0x02800000..0x02FFFFFF (8 MiB).
# In the observed FIT boot, the running U-Boot is relocated above 0x04000000,
# but the firmware's live RAM ownership has not been independently audited.
# Do NOT enable without a cold-boot recovery image and board-specific review.
option(LEXXOS_RK3506_DDR_EXTENDED
        "EXPERIMENTAL: reclaim 8 MiB DDR for paged application workspaces" OFF)
if (LEXXOS_RK3506_DDR_EXTENDED)
    if (NOT LEXXOS_RK3506_MMU_MANAGED OR NOT LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
        message(FATAL_ERROR "DDR_EXTENDED requires RK3506 managed MMU and FreeRTOS")
    endif ()
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_DDR_EXTENDED=1)
    message(WARNING "RK3506 extended DDR (0x02800000..0x02FFFFFF) is experimental; verify firmware ownership")
endif ()

# DMA is explicitly opted in after the managed-MMU/FreeRTOS memory tests.
# PL330 receives only non-cacheable physical buffers and microcode.
option(LEXXOS_RK3506_DMA_SAFE
        "Enable RK3506 managed-MMU PL330 SPI0/SPI1 DMA (experimental)" OFF)
if (LEXXOS_RK3506_DMA_SAFE)
    if (NOT LEXXOS_RK3506_MMU_MANAGED
            OR NOT LEXXOS_RK3506_FREERTOS_PERIPHERALS
            OR NOT LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
        message(FATAL_ERROR
                "RK3506 DMA_SAFE requires managed MMU and FreeRTOS peripherals")
    endif ()
    target_compile_definitions(lexxos PRIVATE LEXXOS_RK3506_DMA_SAFE=1)

    # Normal-NC LCD staging: larger transfers reduce PL330 launches and
    # FreeRTOS wakeups while retaining the cache-safe bounce-buffer contract.
    set(LEXXOS_RK3506_LCD_DMA_CHUNK_KIB "16" CACHE STRING
            "LCD PL330 Normal-NC staging chunk in KiB (4/8/16/32)")
    set_property(CACHE LEXXOS_RK3506_LCD_DMA_CHUNK_KIB PROPERTY STRINGS 4 8 16 32)
    if (NOT LEXXOS_RK3506_LCD_DMA_CHUNK_KIB MATCHES "^(4|8|16|32)$")
        message(FATAL_ERROR "RK3506 LCD DMA chunk must be 4, 8, 16 or 32 KiB")
    endif ()
    math(EXPR _rk3506_lcd_dma_chunk_bytes "${LEXXOS_RK3506_LCD_DMA_CHUNK_KIB} * 1024")
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_DMA_SAFE=1
            LEXXOS_RK3506_LCD_DMA_CHUNK_BYTES=${_rk3506_lcd_dma_chunk_bytes})
    message(STATUS "RK3506 LCD DMA staging: ${LEXXOS_RK3506_LCD_DMA_CHUNK_KIB} KiB")

    # PL330 LCD RGB565: burst=2 passed on-device tests (~5.2 MiB/s).
    # Keep 1 for A/B regression and 4 for further experimentation.
    set(LEXXOS_RK3506_LCD_DMA_BURST_UNITS "2" CACHE STRING
            "EXPERIMENTAL: PL330 LCD RGB565 burst transfers per request (1/2/4)")
    set_property(CACHE LEXXOS_RK3506_LCD_DMA_BURST_UNITS PROPERTY STRINGS 1 2 4)
    if (NOT LEXXOS_RK3506_LCD_DMA_BURST_UNITS MATCHES "^(1|2|4)$")
        message(FATAL_ERROR "RK3506 LCD DMA burst must be 1, 2 or 4")
    endif ()
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_LCD_DMA_BURST_UNITS=${LEXXOS_RK3506_LCD_DMA_BURST_UNITS})
    message(STATUS "RK3506 LCD DMA burst: ${LEXXOS_RK3506_LCD_DMA_BURST_UNITS} beats/request")

    # A/B option: avoid FreeRTOS 1 ms sleeps between PL330 IRQ148 and
    # presentation. WFE also wakes on timer PPI27; no GIC priority changes.
    option(LEXXOS_RK3506_LCD_DMA_WAIT_WFE
            "Experimental: wait for LCD DMA IRQ148 with ARM WFE instead of vTaskDelay" OFF)
    if (LEXXOS_RK3506_LCD_DMA_WAIT_WFE)
        target_compile_definitions(lexxos PRIVATE
                LEXXOS_RK3506_LCD_DMA_WAIT_WFE=1)
    endif ()

    # Separate completion semaphore: lcd-present's default task notification
    # already belongs to frame publication and must not be consumed here.
    option(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR
            "Experimental: FreeRTOS LCD DMA completion via IRQ148 semaphore" OFF)
    if (LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR AND LEXXOS_RK3506_LCD_DMA_WAIT_WFE)
        message(FATAL_ERROR "LCD DMA NOTIFY_ISR and WAIT_WFE are mutually exclusive")
    endif ()
    if (LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
        target_compile_definitions(lexxos PRIVATE
                LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR=1)
    endif ()

    # Poll only the short SPI FIFO tail, then yield via the old wait callback.
    set(LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US "200" CACHE STRING
            "LCD DMA FIFO drain busy-poll window in us (0/50/100/200/500)")
    set_property(CACHE LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US PROPERTY STRINGS 0 50 100 200 500)
    if (NOT LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US MATCHES "^(0|50|100|200|500)$")
        message(FATAL_ERROR "RK3506 LCD DMA drain spin must be 0, 50, 100, 200 or 500 us")
    endif ()
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US=${LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US})
    message(STATUS "RK3506 LCD DMA: completion WFE=${LEXXOS_RK3506_LCD_DMA_WAIT_WFE}, drain spin=${LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US} us")
    message(WARNING "RK3506 PL330 DMA is experimental: keep a known-good boot image")
endif ()


if (LEXXOS_RK3506_MMU_NORMAL_NC)
    if (NOT LEXXOS_BOARD_PICOCALC_LYRA)
        message(FATAL_ERROR
                "LEXXOS_RK3506_MMU_NORMAL_NC is only defined for 128 MiB PicoCalc Lyra")
    endif ()
    target_sources(lexxos PRIVATE "${_rk3506_source}/mmu_normal_nc.cpp")
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_MMU_NORMAL_NC=1)
endif ()

target_include_directories(lexxos PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/sdk"
        "${LEXXOS_GENERATED_INCLUDE_DIR}")

# NativeTransport uses the shared Lexx UI wire types, not guest RV32I syscalls.
target_compile_definitions(lexxos PRIVATE LEXXOS_UI_NATIVE=1)

# Debug без оптимизации создаёт вызовы функций на каждый байт SPI и пиксель.
# Оставляем остальной runtime отлаживаемым, а LCD hot path собираем с -O2.
# OFF нужен для сопоставимого A/B замера; частоты, DMA и IRQ не меняются.
option(LEXXOS_RK3506_LCD_OPTIMIZED "Optimize LCD transfer and pixel conversion even in Debug" ON)
if (LEXXOS_RK3506_SHELL AND LEXXOS_RK3506_LCD_OPTIMIZED)
    set_property(SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_spi0.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_spi_controller.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_lcd_spi.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/composition/products/lyra_lcd_dma.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_dma_pl330.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_dma_controller.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_tx_program.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_renderer.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_text_renderer.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_terminal_state.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_screen_buffer.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_viewport.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_font.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_status_bar_renderer.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/display/double_buffered_display.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/display/dirty_region_set.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/display/detail/display_frame_guard.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/display/legacy_ili9488_backend.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/devices/display/ili9488_display.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_pixels.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_stream.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -O2)
endif ()

# Keep the RV32I dispatch hot path optimized in Debug builds.
# OFF restores the original compiler behavior for A/B measurements.
# This does not change the guest ABI or the FreeRTOS port.
option(LEXXOS_RK3506_VM_OPTIMIZED
        "Optimize the RV32I interpreter in Debug builds" ON)

set(LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL "3" CACHE STRING
        "Optimization level for the RK3506 RV32I interpreter: 2 or 3")
set_property(CACHE LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL
        PROPERTY STRINGS 2 3)
if (NOT LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL MATCHES "^(2|3)$")
    message(FATAL_ERROR
            "LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL must be 2 or 3")
endif ()

if (LEXXOS_RK3506_SHELL AND LEXXOS_RK3506_VM_OPTIMIZED)
    set_property(SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/external_program_runner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/ui/guest_ui_tree_layout.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/lexx_resources.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/ui/guest_ui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_support.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_virtual_machine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_devices.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_graphics.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_io.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexxos/external_program/detail/rv32i_syscalls.cpp"
            APPEND PROPERTY COMPILE_OPTIONS
            "-O${LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL}")
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL=${LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL})
endif ()

# Software-only interpreter fast loop, disable for A/B testing.
option(LEXXOS_RK3506_VM_FAST_LOOP
        "Batch RV32I instruction dispatch and bypass the zero register" ON)

# Preserve the legacy option as the AUTO-profile default.
# Explicit LEXXOS_VM_PROFILE overrides it.
include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/VMExecution.cmake")
lexxos_configure_vm_target(lexxos "rk3506")

option(LEXXOS_RK3506_VM_ALIGNED_ACCESS
        "Use aligned native halfword/word accesses in the RV32I VM"
        ON)
if (LEXXOS_RK3506_SHELL AND LEXXOS_RK3506_VM_ALIGNED_ACCESS)
    target_compile_definitions(lexxos PRIVATE
            LEXXOS_RK3506_VM_ALIGNED_ACCESS=1)
endif ()

if (NOT LEXXOS_SYSTEM_SDK AND LEXXOS_RK3506_SHELL)
    target_sources(lexxos PRIVATE "${_rk3506_source}/shell_entry.cpp")
endif ()

target_compile_features(lexxos PRIVATE cxx_std_20)
target_compile_options(lexxos PRIVATE
        -mcpu=cortex-a7 -marm -mfloat-abi=soft
        # RK3506 is booted from U-Boot with an inherited SCTLR value.
        # Keep generated RAM accesses alignment-safe even with -Os/-O2.
        -mno-unaligned-access
        -ffreestanding -fno-builtin -ffunction-sections -fdata-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
        -Wall -Wextra
)
set(_rk3506_link_scope PRIVATE)
if (LEXXOS_SYSTEM_SDK)
    set(_rk3506_link_scope INTERFACE)
    target_link_options(lexxos INTERFACE -Wl,-u,_start -Wl,-u,_sbrk)
    set_property(TARGET lexxos PROPERTY INTERFACE_LINK_DEPENDS "${_rk3506_source}/linker.ld")
else ()
    target_link_options(lexxos PRIVATE "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/lexxos.map")
endif ()
target_link_options(lexxos ${_rk3506_link_scope}
        -mcpu=cortex-a7 -marm -mfloat-abi=soft
        -nostdlib -nostartfiles
        -Wl,--gc-sections -Wl,--build-id=none
        "-T${_rk3506_source}/linker.ld"
)

if (LEXXOS_SYSTEM_SDK)
    target_include_directories(lexxos INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/sdk"
            "${LEXXOS_GENERATED_INCLUDE_DIR}")
    target_compile_features(lexxos INTERFACE cxx_std_20)
    target_compile_definitions(lexxos INTERFACE
            LEXXOS_PLATFORM_RK3506=1 LEXXOS_UI_NATIVE=1 LEXXOS_BOARD_NAME="${LEXXOS_BOARD}")
    if (LEXXOS_RK3506_SCHEDULER STREQUAL "freertos")
        target_compile_definitions(lexxos INTERFACE
                LEXXOS_KERNEL_FREERTOS=1 LEXXOS_RK3506_FREERTOS=1
                LEXXOS_RK3506_FREERTOS_HEAP_BYTES=${_rk3506_freertos_heap_bytes})
        target_include_directories(lexxos SYSTEM INTERFACE "${FREERTOS_KERNEL_PATH}/include" "${_rk3506_freertos_port}")
    endif ()
    target_compile_options(lexxos INTERFACE -mcpu=cortex-a7 -marm -mfloat-abi=soft -mno-unaligned-access
            -ffreestanding $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
    add_library(LexxSDK::LexxSDK ALIAS lexxos)
    return()
endif ()

if (NOT CMAKE_OBJCOPY)
    message(FATAL_ERROR "CMAKE_OBJCOPY is required (arm-none-eabi-objcopy)")
endif ()

add_custom_command(TARGET lexxos POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:lexxos>" "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rk3506_pack_uimage.py"
        --input "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
        --output "${CMAKE_CURRENT_BINARY_DIR}/lexxos.uimg"
        --load 0x02000000 --entry 0x02000000
        VERBATIM
        COMMENT "Packaging ARMv7-A bootstrap as U-Boot legacy uImage"
)

# Optional override for the retained board FIT components in ports/lyra.
# The resulting lexxos.img is a full raw FIT container suitable for the
# existing Rockchip `boot` partition, not a filesystem image.
set(LEXXOS_RK3506_FIT_TEMPLATE "" CACHE FILEPATH
        "Path to the stock Luckfox Lyra MircoSD boot.img FIT template")
if (NOT LEXXOS_RK3506_FIT_TEMPLATE STREQUAL "")
    get_filename_component(_rk3506_fit_template "${LEXXOS_RK3506_FIT_TEMPLATE}" ABSOLUTE)
    if (NOT EXISTS "${_rk3506_fit_template}")
        message(FATAL_ERROR "RK3506 FIT template not found: ${_rk3506_fit_template}")
    endif ()
    set(_rk3506_fit_image "${CMAKE_CURRENT_BINARY_DIR}/lexxos.img")
    add_custom_command(
            OUTPUT "${_rk3506_fit_image}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rk3506_pack_fit.py"
            --template "${_rk3506_fit_template}"
            --input "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
            --output "${_rk3506_fit_image}"
            --address 0x02000000
            DEPENDS lexxos "${_rk3506_fit_template}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/rk3506_pack_fit.py"
            VERBATIM
            COMMENT "Packing RK3506 boot partition FIT image lexxos.img"
    )
    add_custom_target(lexxos_fit ALL DEPENDS "${_rk3506_fit_image}")
else ()
    set(_rk3506_fit_image "${CMAKE_CURRENT_BINARY_DIR}/lexxos.img")
    file(GLOB _lyra_vendor_components CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rk3506-luckfox-lurra/*.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rk3506-luckfox-lurra/*.dtb"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rk3506-luckfox-lurra/resource.img")
    add_custom_command(OUTPUT "${_rk3506_fit_image}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ports/lyra/build.py"
            --boot-only --kernel "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
            --output "${_rk3506_fit_image}"
            DEPENDS lexxos ${_lyra_vendor_components}
            "${CMAKE_CURRENT_SOURCE_DIR}/ports/lyra/components.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/ports/lyra/build.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/rk3506_pack_fit.py"
            VERBATIM COMMENT "Packing LexxOS FIT with pinned Lyra board resources")
    add_custom_target(lexxos_fit ALL DEPENDS "${_rk3506_fit_image}")
endif ()

# U-Boot is built independently of the bare-metal application, on the host.
# Docker and downloads are needed only when this explicit target is invoked.
set(_rk3506_uboot_dir "${CMAKE_CURRENT_BINARY_DIR}/uboot")
add_custom_target(uboot-rk3506
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rk3506_build_uboot.py"
        --build-dir "${_rk3506_uboot_dir}"
        BYPRODUCTS "${_rk3506_uboot_dir}/uboot.img"
        USES_TERMINAL VERBATIM
        COMMENT "Building PicoCalc Lyra U-Boot with splash (Docker; no device writes)"
)
option(LEXXOS_RK3506_FLASH_BUILT_UBOOT
        "Build U-Boot and include it in update-rk3506 (writes only if changed)" OFF)

# Deliberately separate from ALL: a normal build NEVER flashes a disk.
# Interactive selection is the default; an explicit whole-disk name may be
# provided as LEXXOS_RK3506_FLASH_DISK=/dev/diskN.
set(LEXXOS_RK3506_UBOOT_IMAGE "" CACHE FILEPATH
        "Optional board-compatible uboot.img; flash only when SD contents differ")
set(LEXXOS_RK3506_FLASH_DISK "" CACHE STRING "Optional /dev/diskN for interactive macOS SD flashing")
if (TARGET lexxos_fit)
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(_rk3506_flash_args)
        set(_rk3506_flash_deps lexxos_fit)
        if (LEXXOS_RK3506_FLASH_BUILT_UBOOT)
            if (NOT LEXXOS_RK3506_UBOOT_IMAGE STREQUAL "")
                message(FATAL_ERROR "Select either FLASH_BUILT_UBOOT or a custom UBOOT_IMAGE, not both")
            endif ()
            list(APPEND _rk3506_flash_args --uboot-image "${_rk3506_uboot_dir}/uboot.img")
            list(APPEND _rk3506_flash_deps uboot-rk3506)
        elseif (NOT LEXXOS_RK3506_UBOOT_IMAGE STREQUAL "")
            get_filename_component(_rk3506_uboot_image "${LEXXOS_RK3506_UBOOT_IMAGE}" ABSOLUTE
                    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            if (NOT EXISTS "${_rk3506_uboot_image}")
                message(FATAL_ERROR
                        "LEXXOS_RK3506_UBOOT_IMAGE does not exist: ${_rk3506_uboot_image}. "
                        "Set a real board-compatible uboot.img, or clear this option for boot-only flashing. "
                        "update-rk3506 does not build U-Boot; see ports/uboot/picocalc_lyra/README.md")
            endif ()
            list(APPEND _rk3506_flash_args --uboot-image "${_rk3506_uboot_image}")
            list(APPEND _rk3506_flash_deps "${_rk3506_uboot_image}")
        endif ()
        if (NOT LEXXOS_RK3506_FLASH_DISK STREQUAL "")
            list(APPEND _rk3506_flash_args --disk "${LEXXOS_RK3506_FLASH_DISK}")
        endif ()
        add_custom_target(update-rk3506
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/flash_rk3506_macos.py"
                --image "${_rk3506_fit_image}"
                --backup-dir "${CMAKE_CURRENT_SOURCE_DIR}/work/backups/rk3506"
                ${_rk3506_flash_args}
                DEPENDS ${_rk3506_flash_deps}
                USES_TERMINAL VERBATIM
                COMMENT "INTERACTIVE: backup and flash Lyra SD boot partition; system disks blocked"
        )
        add_custom_target(list-rk3506-update-disks
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/flash_rk3506_macos.py"
                --image "${_rk3506_fit_image}"
                --backup-dir "${CMAKE_CURRENT_SOURCE_DIR}/work/backups/rk3506" --list
                USES_TERMINAL VERBATIM
                COMMENT "List recognized removable Luckfox Lyra SD cards (read-only)"
        )
    endif ()
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/ports/lyra/targets.cmake")

# Emit static frame sizes alongside objects for stack-budget audits.
target_compile_options(lexxos PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:-fstack-usage>")
