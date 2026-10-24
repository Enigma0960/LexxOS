lexxos_validate_target()

function(lexxos_default_sd_backend_for_board output_variable)
    if (LEXXOS_PLATFORM STREQUAL "sim")
        set(${output_variable} "sim" PARENT_SCOPE)
    elseif (LEXXOS_BOARD STREQUAL "euse_m4_demo_stm32f407")
        set(${output_variable} "sdio" PARENT_SCOPE)
    elseif (LEXXOS_BOARD STREQUAL "dev_pico_sdio")
        set(${output_variable} "sdio" PARENT_SCOPE)
    elseif (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
        set(${output_variable} "sdio" PARENT_SCOPE)
    elseif (LEXXOS_BOARD STREQUAL "dev_pico"
            OR LEXXOS_BOARD STREQUAL "dev_pico2w"
            OR (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
            OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2")
    )
        set(${output_variable} "spi" PARENT_SCOPE)
    elseif (LEXXOS_PLATFORM STREQUAL "stm32f4")
        set(${output_variable} "sdio" PARENT_SCOPE)
    else ()
        set(${output_variable} "spi" PARENT_SCOPE)
    endif ()
endfunction()

set(LEXXOS_EFFECTIVE_SD_BACKEND "${LEXXOS_SD_BACKEND}")
if (LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "")
    lexxos_default_sd_backend_for_board(LEXXOS_EFFECTIVE_SD_BACKEND)
endif ()

if (NOT LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "spi"
        AND NOT LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sdio"
        AND NOT LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sim")
    message(FATAL_ERROR "Unsupported LEXXOS_SD_BACKEND='${LEXXOS_SD_BACKEND}'. Use empty, spi or sdio.")
endif ()

if (LEXXOS_PLATFORM STREQUAL "pico"
        AND LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sdio"
        AND NOT LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO)
    message(FATAL_ERROR "RP2040 SDIO support is experimental and currently builds only a guarded skeleton. Use LEXXOS_SD_BACKEND=spi or set LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO=ON explicitly.")
endif ()

# Keep the effective common source groups in one shared catalog.
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/lexxos_sources.cmake")

function(lexxos_configure_common_target target_name)
    target_sources(${target_name}
            PRIVATE
            ${LEXXOS_RUNTIME_SOURCES}
            ${LEXXOS_PROGRAM_SOURCES}
            ${LEXXOS_CORE_PROGRAM_SOURCES}
            ${LEXXOS_EDITOR_PROGRAM_SOURCES}
            ${LEXXOS_FILESYSTEM_PROGRAM_SOURCES}
            ${LEXXOS_DEVICE_PROGRAM_SOURCES}
            ${LEXXOS_UTILITY_PROGRAM_SOURCES}
            ${LEXXOS_MIDI_PROGRAM_SOURCES}
            ${LEXXOS_BUS_PROGRAM_SOURCES}
            ${LEXXOS_DEVICE_SOURCES}
            ${LEXXOS_SHELL_SOURCES}
            ${LEXXOS_VFS_SOURCES}
            ${LEXXOS_FAT_SOURCES}
            ${LEXXOS_STORAGE_SOURCES}
            ${LEXXOS_DISPLAY_SOURCES}
            ${LEXXOS_INPUT_SOURCES}
    )

    if (LEXXOS_FEATURE_GAMEBOY)
        target_sources(${target_name}
                PRIVATE
                ${LEXXOS_GAMEBOY_SOURCES}
        )

        if (LEXXOS_GAMEBOY_OPTIMIZE_HOTPATH)
            foreach (gameboy_source IN LISTS LEXXOS_GAMEBOY_SOURCES)
                set_source_files_properties(${gameboy_source}
                        PROPERTIES
                        COMPILE_OPTIONS "-O3;-fno-exceptions;-fno-rtti"
                )
            endforeach ()
        endif ()

        if (LEXXOS_GAMEBOY_ENABLE_LTO)
            target_compile_options(${target_name} PRIVATE -flto)
            target_link_options(${target_name} PRIVATE -flto)
        endif ()

        set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS "${LEXXOS_GAMEBOY_ROM_CACHE_BANKS}")
        if (LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS STREQUAL "")
            if (LEXXOS_PLATFORM STREQUAL "pico")
                set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS 2)
            else ()
                set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS 4)
            endif ()
        endif ()

        set(LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET "${LEXXOS_GAMEBOY_FLASH_ROM_OFFSET}")
        if (LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET STREQUAL "")
            if ((LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2") OR LEXXOS_BOARD STREQUAL "dev_pico2w")
                # Pico 2 W images with Wi-Fi, FatFs, display, gamepad and gbemu can
                # exceed 1 MiB. Keep the ROM staging area away from the firmware image.
                set(LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET 2097152)
            elseif (LEXXOS_PLATFORM STREQUAL "pico")
                set(LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET 1048576)
            endif ()
        endif ()
        if (NOT LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET STREQUAL "")
            target_compile_definitions(${target_name} PRIVATE LEXXOS_GAMEBOY_FLASH_ROM_OFFSET=${LEXXOS_EFFECTIVE_GAMEBOY_FLASH_ROM_OFFSET})
        endif ()


        set(LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES "${LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES}")
        target_compile_definitions(${target_name}
                PRIVATE
                LEXXOS_GAMEBOY_ROM_CACHE_BANKS=${LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_BANKS}
        )
        if (NOT LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES STREQUAL "")
            target_compile_definitions(${target_name}
                    PRIVATE LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=${LEXXOS_EFFECTIVE_GAMEBOY_ROM_CACHE_PAGE_BYTES})
        endif ()
    endif ()

    set(LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS "${LEXXOS_FAT_READ_CACHE_SECTORS}")
    set(LEXXOS_EFFECTIVE_FAT_WRITE_BUFFER_SECTORS "${LEXXOS_FAT_WRITE_BUFFER_SECTORS}")
    set(LEXXOS_EFFECTIVE_FAT_DISK_WRITE_MAX_SECTORS "${LEXXOS_FAT_DISK_WRITE_MAX_SECTORS}")
    set(LEXXOS_EFFECTIVE_FAT_SECTOR_CACHE_SLOTS "${LEXXOS_FAT_SECTOR_CACHE_SLOTS}")
    set(LEXXOS_EFFECTIVE_FAT_CLUSTER_CACHE_ENTRIES "${LEXXOS_FAT_CLUSTER_CACHE_ENTRIES}")
    set(LEXXOS_EFFECTIVE_FAT_MOUNT_SLOTS "${LEXXOS_FAT_MOUNT_SLOTS}")
    if (LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico" AND LEXXOS_FEATURE_GAMEBOY)
            set(LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS 16)
        elseif (LEXXOS_PLATFORM STREQUAL "pico"
                AND (LEXXOS_BOARD STREQUAL "dev_pico2w"
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2")))
            # CYW43 + lwIP adds static RAM pressure on RP2040. Keep a smaller but
            # still useful read-ahead cache for Wi-Fi enabled board profiles.
            set(LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS 32)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS 64)
        endif ()
    endif ()
    if (LEXXOS_EFFECTIVE_FAT_WRITE_BUFFER_SECTORS STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico")
            set(LEXXOS_EFFECTIVE_FAT_WRITE_BUFFER_SECTORS 8)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_WRITE_BUFFER_SECTORS 16)
        endif ()
    endif ()
    if (LEXXOS_EFFECTIVE_FAT_DISK_WRITE_MAX_SECTORS STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico")
            set(LEXXOS_EFFECTIVE_FAT_DISK_WRITE_MAX_SECTORS 8)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_DISK_WRITE_MAX_SECTORS 0)
        endif ()
    endif ()
    if (LEXXOS_EFFECTIVE_FAT_SECTOR_CACHE_SLOTS STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico")
            set(LEXXOS_EFFECTIVE_FAT_SECTOR_CACHE_SLOTS 1)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_SECTOR_CACHE_SLOTS 4)
        endif ()
    endif ()
    if (LEXXOS_EFFECTIVE_FAT_CLUSTER_CACHE_ENTRIES STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico")
            set(LEXXOS_EFFECTIVE_FAT_CLUSTER_CACHE_ENTRIES 64)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_CLUSTER_CACHE_ENTRIES 256)
        endif ()
    endif ()
    if (LEXXOS_EFFECTIVE_FAT_MOUNT_SLOTS STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico")
            set(LEXXOS_EFFECTIVE_FAT_MOUNT_SLOTS 1)
        else ()
            set(LEXXOS_EFFECTIVE_FAT_MOUNT_SLOTS 2)
        endif ()
    endif ()

    target_include_directories(${target_name}
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${LEXXOS_GENERATED_INCLUDE_DIR}
    )
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if (LEXXOS_FEATURE_FATFS)
        target_link_libraries(${target_name} PRIVATE fatfs)
    endif ()

    target_compile_definitions(${target_name}
            PRIVATE
            LEXXOS_BOARD_NAME="${LEXXOS_BOARD}"
            LEXXOS_FAT_READ_CACHE_SECTORS=${LEXXOS_EFFECTIVE_FAT_READ_CACHE_SECTORS}
            LEXXOS_FAT_WRITE_BUFFER_SECTORS=${LEXXOS_EFFECTIVE_FAT_WRITE_BUFFER_SECTORS}
            LEXXOS_FAT_DISK_WRITE_MAX_SECTORS=${LEXXOS_EFFECTIVE_FAT_DISK_WRITE_MAX_SECTORS}
            LEXXOS_FAT_SECTOR_CACHE_SLOTS=${LEXXOS_EFFECTIVE_FAT_SECTOR_CACHE_SLOTS}
            LEXXOS_FAT_CLUSTER_CACHE_ENTRIES=${LEXXOS_EFFECTIVE_FAT_CLUSTER_CACHE_ENTRIES}
            LEXXOS_FAT_MOUNT_SLOTS=${LEXXOS_EFFECTIVE_FAT_MOUNT_SLOTS}
    )

    if (LEXXOS_FEATURE_SD_CARD AND LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "spi")
        target_compile_definitions(${target_name} PRIVATE LEXXOS_SD_BACKEND_SPI=1)
    elseif (LEXXOS_FEATURE_SD_CARD AND LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sdio")
        target_compile_definitions(${target_name} PRIVATE LEXXOS_SD_BACKEND_SDIO=1)
    endif ()

    if (LEXXOS_PLATFORM STREQUAL "pico" AND LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO)
        target_compile_definitions(${target_name} PRIVATE LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO=1)
    endif ()

    if (LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS)
        target_compile_definitions(${target_name} PRIVATE LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS=1)
    endif ()

    set(LEXXOS_EFFECTIVE_ILI9488_RGB565_WIRE OFF)
    string(TOLOWER "${LEXXOS_ILI9488_RGB565_WIRE}" LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED)
    if (LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "auto"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "")
        if (LEXXOS_PLATFORM STREQUAL "pico"
                AND ((LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2")))
            set(LEXXOS_EFFECTIVE_ILI9488_RGB565_WIRE ON)
        endif ()
    elseif (LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "on"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "true"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "yes"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "1")
        set(LEXXOS_EFFECTIVE_ILI9488_RGB565_WIRE ON)
    elseif (LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "off"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "false"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "no"
            OR LEXXOS_ILI9488_RGB565_WIRE_NORMALIZED STREQUAL "0")
        set(LEXXOS_EFFECTIVE_ILI9488_RGB565_WIRE OFF)
    else ()
        message(FATAL_ERROR "Unsupported LEXXOS_ILI9488_RGB565_WIRE='${LEXXOS_ILI9488_RGB565_WIRE}'. Use auto, on or off.")
    endif ()
    if (LEXXOS_EFFECTIVE_ILI9488_RGB565_WIRE)
        target_compile_definitions(${target_name} PRIVATE LEXXOS_ILI9488_RGB565_WIRE=1)
    endif ()
endfunction()

if (LEXXOS_PLATFORM STREQUAL "stm32f4")
    add_executable(lexxos)
    target_sources(lexxos
            PRIVATE
            lexxos/platform/stm32/bringup/startup_stm32f407xx.c
            lexxos/platform/stm32/bringup/system_stm32f407xx.c
    )

    target_include_directories(lexxos PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${LEXXOS_GENERATED_INCLUDE_DIR})

    target_compile_definitions(lexxos
            PRIVATE
            LEXXOS_PLATFORM_STM32F4=1
            LEXXOS_BOARD_NAME="${LEXXOS_BOARD}"
    )
    target_compile_options(lexxos
            PRIVATE
            -mcpu=cortex-m4
            -mthumb
            -mfpu=fpv4-sp-d16
            -mfloat-abi=hard
            -ffunction-sections
            -fdata-sections
            -fno-common
            -Wall
            -Wextra
    )
    target_link_options(lexxos
            PRIVATE
            -mcpu=cortex-m4
            -mthumb
            -mfpu=fpv4-sp-d16
            -mfloat-abi=hard
            -T${CMAKE_CURRENT_LIST_DIR}/lexxos/platform/stm32/linker/stm32f407zg.ld
            -Wl,--gc-sections
            -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/lexxos.map
            -nostartfiles
    )

    if (LEXXOS_STM32F4_BRINGUP_ONLY)
        target_sources(lexxos
                PRIVATE
                lexxos/platform/stm32/bringup/main.c
        )
    else ()
        lexxos_configure_common_target(lexxos)
        target_compile_definitions(lexxos PRIVATE LEXXOS_KERNEL_FREERTOS=1)
        target_compile_options(lexxos
                PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
                $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
                $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
        )
        target_sources(lexxos
                PRIVATE
                main.cpp
                lexxos/platform/stm32/board_euse_m4_demo_stm32f407.cpp
                lexxos/platform/stm32/detail/stm32_console.cpp
                lexxos/platform/stm32/detail/stm32_gpio.cpp
                lexxos/platform/stm32/runtime/platform_stm32f4.cpp
                lexxos/platform/stm32/runtime/syscalls.c
                lexxos/platform/stm32/runtime/cxx_runtime.cpp
                lexxos/platform/stm32/runtime/freertos_heap.c
                lexxos/devices/stm32/stm32_placeholder_i2c_bus.cpp
                lexxos/devices/stm32/i2c_bus_stm32.cpp
                lexxos/devices/stm32/stm32_placeholder_spi_bus.cpp
                lexxos/devices/stm32/spi_bus_stm32.cpp
                lexxos/kernel/freertos/hooks.cpp
                lexxos/kernel/freertos/scheduler_freertos.cpp
        )

        if (LEXXOS_FEATURE_SD_CARD AND LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "sdio")
            target_sources(lexxos
                    PRIVATE
                    lexxos/devices/stm32/detail/stm32_sdio_support.cpp
                    lexxos/devices/stm32/sd_card_sdio_stm32.cpp
            )
        endif ()
        if (LEXXOS_FEATURE_DISPLAY)
            target_sources(lexxos
                    PRIVATE
                    lexxos/devices/stm32/detail/stm32_parallel_tft_support.cpp
                    lexxos/devices/stm32/parallel_tft_display_stm32.cpp
            )
        endif ()

        target_link_libraries(lexxos
                PRIVATE
                FreeRTOS-Config
                FreeRTOS-Kernel
                FreeRTOS-Kernel-Heap4
        )
    endif ()

    set_target_properties(lexxos PROPERTIES SUFFIX ".elf")

    add_custom_command(TARGET lexxos POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:lexxos>
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:lexxos> ${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:lexxos> ${CMAKE_CURRENT_BINARY_DIR}/lexxos.hex
            COMMENT "Generating STM32F407 binary and Intel HEX images"
    )
elseif (LEXXOS_PLATFORM STREQUAL "sim")
    find_package(Threads REQUIRED)
    add_library(lexxos_sim SHARED)
    lexxos_configure_common_target(lexxos_sim)
    target_compile_definitions(lexxos_sim PRIVATE LEXXOS_PLATFORM_SIM=1)
    target_include_directories(lexxos_sim
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/../emulator/native
    )
    target_sources(lexxos_sim
            PRIVATE
            ../emulator/native/lexxos_sim_api.cpp
            ../emulator/native/platform_sim.cpp
            ../emulator/native/scheduler_sim.cpp
            ../emulator/native/sim_audio_device.cpp
            ../emulator/native/sim_i2c_bus.cpp
            ../emulator/native/sim_runtime.cpp
    )

    if (LEXXOS_FEATURE_DISPLAY)
        target_sources(lexxos_sim PRIVATE ../emulator/native/sim_display_device.cpp)
    endif ()
    if (LEXXOS_FEATURE_KEYBOARD)
        target_sources(lexxos_sim PRIVATE ../emulator/native/sim_input_device.cpp)
    endif ()
    if (LEXXOS_FEATURE_SD_CARD)
        target_sources(lexxos_sim PRIVATE ../emulator/native/sim_sd_card.cpp)
    endif ()

    target_link_libraries(lexxos_sim PRIVATE Threads::Threads)
    set_target_properties(lexxos_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)
else ()
    add_executable(lexxos)
    lexxos_configure_common_target(lexxos)
    target_sources(lexxos
            PRIVATE
            main.cpp
    )

    if (LEXXOS_PLATFORM STREQUAL "host")
        target_compile_definitions(lexxos PRIVATE LEXXOS_PLATFORM_HOST=1)
        target_sources(lexxos
                PRIVATE
                lexxos/platform/host/platform_host.cpp
                lexxos/devices/host/host_i2c_bus.cpp
                lexxos/devices/host/i2c_bus_host.cpp
                lexxos/devices/host/host_spi_bus.cpp
                lexxos/devices/host/spi_bus_host.cpp
                lexxos/kernel/host/scheduler_host.cpp
        )
    elseif (LEXXOS_PLATFORM STREQUAL "pico")
        target_compile_definitions(lexxos PRIVATE
                LEXXOS_PLATFORM_PICO=1
                PICO_MAX_SHARED_IRQ_HANDLERS=8
        )
        target_sources(lexxos
                PRIVATE
                lexxos/platform/pico/platform_pico.cpp
                lexxos/platform/pico/detail/pico_console.cpp
                lexxos/platform/pico/detail/pico_flash.cpp
                lexxos/platform/pico/detail/pico_psram.cpp
                lexxos/platform/pico/detail/pico_gpio.cpp
                lexxos/platform/pico/detail/pico_gpio_interrupts.cpp
                lexxos/platform/pico/stdio_wrap_stubs.cpp
                lexxos/devices/pico/pico_i2c_bus.cpp
                lexxos/devices/pico/i2c_bus_pico.cpp
                lexxos/devices/pico/pico_spi_bus.cpp
                lexxos/devices/pico/spi_bus_pico.cpp
        )

        if (LEXXOS_PICO_USE_FREERTOS)
            target_compile_definitions(lexxos PRIVATE LEXXOS_KERNEL_FREERTOS=1)
            target_sources(lexxos
                    PRIVATE
                    lexxos/kernel/freertos/hooks.cpp
                    lexxos/kernel/freertos/scheduler_freertos.cpp
                    lexxos/platform/pico/detail/pico_freertos_exceptions.cpp
            )
            target_link_libraries(lexxos
                    PRIVATE
                    FreeRTOS-Config
                    FreeRTOS-Kernel
                    FreeRTOS-Kernel-Heap4
            )
        else ()
            target_sources(lexxos
                    PRIVATE
                    lexxos/kernel/pico/scheduler_bootstrap.cpp
            )
        endif ()

        if (LEXXOS_BOARD STREQUAL "dev_pico")
            target_sources(lexxos PRIVATE lexxos/platform/pico/board_dev_pico.cpp)
        elseif (LEXXOS_BOARD STREQUAL "dev_pico_sdio")
            target_sources(lexxos PRIVATE lexxos/platform/pico/board_dev_pico_sdio.cpp)
        elseif ((LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico"))
            target_sources(lexxos PRIVATE lexxos/platform/pico/board_picocalc_pico_w.cpp)
        elseif (LEXXOS_BOARD STREQUAL "dev_pico2w")
            target_sources(lexxos PRIVATE lexxos/platform/pico/board_dev_pico2w.cpp)
        elseif ((LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2"))
            target_sources(lexxos PRIVATE lexxos/platform/pico/board_picocalc_pico2w.cpp)
        else ()
            message(FATAL_ERROR "Unsupported Pico board profile: ${LEXXOS_BOARD}")
        endif ()

        target_link_libraries(lexxos
                PRIVATE
                pico_stdlib
                hardware_gpio
                hardware_i2c
                hardware_spi
                hardware_dma
                pico_sync
                hardware_uart
                hardware_timer
                hardware_watchdog
                hardware_clocks
                hardware_vreg
                hardware_flash
                hardware_pwm
                hardware_pio
                hardware_exception
        )

        if ((LEXXOS_BOARD STREQUAL "dev_pico2w"
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2"))
                AND TARGET pico_cyw43_arch_lwip_threadsafe_background)
            target_link_libraries(lexxos PRIVATE pico_cyw43_arch_lwip_threadsafe_background)
            target_include_directories(lexxos PRIVATE
                    ${LEXXOS_ROOT_DIR}/config
            )
            target_compile_definitions(lexxos PRIVATE
                    LEXXOS_PICO_CYW43_AVAILABLE=1
                    LEXXOS_PICO_CYW43_LWIP_AVAILABLE=1)
        elseif ((LEXXOS_BOARD STREQUAL "dev_pico2w"
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico_w" OR LEXXOS_BOARD STREQUAL "picocalc_pico")
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2"))
                AND TARGET pico_cyw43_arch_none)
            target_link_libraries(lexxos PRIVATE pico_cyw43_arch_none)
            target_compile_definitions(lexxos PRIVATE LEXXOS_PICO_CYW43_AVAILABLE=1)
        endif ()

        set(LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ "${LEXXOS_PICO_SYS_CLOCK_KHZ}")
        if (LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ STREQUAL ""
                AND LEXXOS_FEATURE_GAMEBOY
                AND (LEXXOS_BOARD STREQUAL "dev_pico2w"
                OR (LEXXOS_BOARD STREQUAL "picocalc_pico2w" OR LEXXOS_BOARD STREQUAL "picocalc_pico2")))
            set(LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ 250000)
        endif ()
        if (NOT LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ STREQUAL "")
            target_compile_definitions(lexxos PRIVATE LEXXOS_PICO_SYS_CLOCK_KHZ=${LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ})
            if (TARGET FreeRTOS-Config)
                target_compile_definitions(FreeRTOS-Config INTERFACE LEXXOS_PICO_SYS_CLOCK_KHZ=${LEXXOS_EFFECTIVE_PICO_SYS_CLOCK_KHZ})
            endif ()
        endif ()

        if (NOT LEXXOS_ST7735_SPI_BAUDRATE STREQUAL "")
            target_compile_definitions(lexxos PRIVATE LEXXOS_ST7735_SPI_BAUDRATE=${LEXXOS_ST7735_SPI_BAUDRATE})
        endif ()

        if (NOT LEXXOS_PICOCALC_SD_SPI_BAUDRATE STREQUAL "")
            target_compile_definitions(lexxos PRIVATE LEXXOS_PICOCALC_SD_SPI_BAUDRATE=${LEXXOS_PICOCALC_SD_SPI_BAUDRATE})
        endif ()

        if (NOT LEXXOS_PICOCALC_WIFI_AUTOCONNECT STREQUAL "")
            target_compile_definitions(lexxos PRIVATE LEXXOS_PICOCALC_WIFI_AUTOCONNECT=${LEXXOS_PICOCALC_WIFI_AUTOCONNECT})
        endif ()

        if (LEXXOS_BOARD_FAMILY_PICOCALC)
            target_compile_definitions(lexxos PRIVATE
                    LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE=${LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE}
                    LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ=${LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ}
                    LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS=${LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS}
            )
        endif ()

        string(TOLOWER "${LEXXOS_PICO_USB_CDC_CONSOLE}" LEXXOS_PICO_USB_CDC_CONSOLE_NORMALIZED)
        if (LEXXOS_PICO_USB_CDC_CONSOLE_NORMALIZED STREQUAL "auto")
            if (LEXXOS_BOARD_FAMILY_PICOCALC)
                set(LEXXOS_PICO_USB_CDC_CONSOLE_ENABLED ON)
            else ()
                set(LEXXOS_PICO_USB_CDC_CONSOLE_ENABLED OFF)
            endif ()
        elseif (LEXXOS_PICO_USB_CDC_CONSOLE_NORMALIZED STREQUAL "on")
            set(LEXXOS_PICO_USB_CDC_CONSOLE_ENABLED ON)
        elseif (LEXXOS_PICO_USB_CDC_CONSOLE_NORMALIZED STREQUAL "off")
            set(LEXXOS_PICO_USB_CDC_CONSOLE_ENABLED OFF)
        else ()
            message(FATAL_ERROR "Unsupported LEXXOS_PICO_USB_CDC_CONSOLE='${LEXXOS_PICO_USB_CDC_CONSOLE}', expected auto, on or off")
        endif ()

        if (LEXXOS_PICO_USB_CDC_CONSOLE_ENABLED)
            target_compile_definitions(lexxos PRIVATE
                    LEXXOS_PICO_USB_CDC_CONSOLE=1
                    PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
            )

            if (LEXXOS_BOARD_FAMILY_PICOCALC)
                target_compile_definitions(lexxos PRIVATE
                        "USBD_MANUFACTURER=\"LexxOS\""
                        "USBD_PRODUCT=\"LexxOS PicoCalc\""
                )
            endif ()
            pico_enable_stdio_usb(lexxos 1)
        else ()
            pico_enable_stdio_usb(lexxos 0)
        endif ()
        pico_enable_stdio_uart(lexxos 0)
        pico_add_extra_outputs(lexxos)
    endif ()
endif ()
