include_guard(GLOBAL)

set(LEXXOS_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src")
get_filename_component(LEXXOS_SOURCE_ROOT "${LEXXOS_SOURCE_ROOT}" ABSOLUTE)

if (NOT DEFINED LEXXOS_EFFECTIVE_SD_BACKEND OR LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "")
    set(LEXXOS_EFFECTIVE_SD_BACKEND "spi")
endif ()

function(lexxos_make_absolute_sources output_variable)
    set(resolved_sources)
    foreach (source IN LISTS ARGN)
        if (IS_ABSOLUTE "${source}")
            list(APPEND resolved_sources "${source}")
        else ()
            list(APPEND resolved_sources "${LEXXOS_SOURCE_ROOT}/${source}")
        endif ()
    endforeach ()
    set(${output_variable} ${resolved_sources} PARENT_SCOPE)
endfunction()

set(LEXXOS_RUNTIME_SOURCE_FILES
        lexxos/app/app.cpp
        lexxos/app/console_input.cpp
        lexxos/app/console_output.cpp
        lexxos/app/program_input.cpp
        lexxos/app/detail/program_input_queue.cpp
        lexxos/app/runtime.cpp
        lexxos/app/runtime_tasks.cpp
        lexxos/app/software_rtc_task.cpp
        lexxos/app/wifi_autoconnect.cpp
        lexxos/app/console_mirror.cpp
        lexxos/app/detail/null_console_mirror_sink.cpp
        lexxos/app/detail/picocalc_lcd_console_mirror_sink.cpp
        lexxos/app/system_event_service.cpp
        lexxos/app/picocalc_lcd_console.cpp
        lexxos/app/detail/picocalc_lcd_console_font.cpp
        lexxos/app/detail/picocalc_lcd_console_character_queue.cpp
        lexxos/app/detail/picocalc_lcd_console_csi_state.cpp
        lexxos/app/detail/picocalc_lcd_console_layout.cpp
        lexxos/app/detail/picocalc_lcd_console_hardware_scroll.cpp
        lexxos/app/detail/picocalc_lcd_console_scrollback.cpp
        lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.cpp
        lexxos/app/detail/picocalc_lcd_console_screen_buffer.cpp
        lexxos/app/detail/picocalc_lcd_console_status_bar_state.cpp
        lexxos/app/detail/picocalc_lcd_console_terminal_state.cpp
        lexxos/app/detail/picocalc_lcd_console_viewport.cpp
        lexxos/app/detail/picocalc_lcd_console_icon.cpp
        lexxos/app/detail/picocalc_lcd_console_battery_icon.cpp
        lexxos/app/detail/picocalc_lcd_console_wifi_icon.cpp
        lexxos/app/detail/picocalc_status_bar_renderer.cpp
        lexxos/app/detail/picocalc_lcd_console_text_renderer.cpp
        lexxos/app/detail/picocalc_lcd_console_renderer.cpp
        lexxos/app/picocalc_display_power.cpp
        lexxos/lifecycle/event_bus.cpp
        lexxos/kernel/scheduler_backend.cpp
        lexxos/kernel/system.cpp
        lexxos/io/output_stream.cpp
        lexxos/os/app_context.cpp
        lexxos/os/operating_system.cpp
        lexxos/runtime/capabilities.cpp
        lexxos/platform/platform_services.cpp
        lexxos/platform/processor.cpp
        lexxos/tui/tui.cpp
)

set(LEXXOS_PROGRAM_SOURCE_FILES
        lexxos/program/arguments.cpp
        lexxos/program/program.cpp
        lexxos/program/program_manager.cpp
        lexxos/program/program_memory_arena.cpp
        lexxos/program/program_supervisor.cpp
        lexxos/external_program/external_program_runner.cpp
        lexxos/apps/programs.cpp
        lexxos/apps/common/core_programs_support.cpp
        lexxos/apps/common/pretty_table.cpp
)

set(LEXXOS_CORE_PROGRAM_SOURCE_FILES)

set(LEXXOS_EDITOR_PROGRAM_SOURCE_FILES)

set(LEXXOS_FILESYSTEM_PROGRAM_SOURCE_FILES
        lexxos/apps/fs/detail/text_file_writer.cpp
)

set(LEXXOS_DEVICE_PROGRAM_SOURCE_FILES
        lexxos/apps/devices/detail/device_program_support.cpp
)

if (LEXXOS_FEATURE_DISPLAY)
    list(APPEND LEXXOS_DEVICE_PROGRAM_SOURCE_FILES
            lexxos/apps/devices/detail/display_program_support.cpp
    )
endif ()

if (LEXXOS_FEATURE_KEYBOARD_TEST_COMMANDS)
    list(APPEND LEXXOS_DEVICE_PROGRAM_SOURCE_FILES
            lexxos/apps/devices/detail/input_program_support.cpp
    )
endif ()

set(LEXXOS_UTILITY_PROGRAM_SOURCE_FILES)

set(LEXXOS_MIDI_PROGRAM_SOURCE_FILES)

set(LEXXOS_BUS_PROGRAM_SOURCE_FILES
        lexxos/apps/detail/i2c_program_support.cpp
        lexxos/apps/rtc/detail/rtc_program_support.cpp
        lexxos/apps/rtc/detail/rtc_settings_store.cpp
)

# Network services are shared by commands and boot-time RTC/Wi-Fi tasks.
set(LEXXOS_NETWORK_PROGRAM_SOURCE_FILES
        lexxos/apps/network/network_time.cpp
        lexxos/apps/network/detail/wifi_credentials_store.cpp
        lexxos/apps/network/detail/wifi_program_support.cpp
)

if (LEXXOS_PLATFORM STREQUAL "espidf")
    list(APPEND LEXXOS_BUS_PROGRAM_SOURCE_FILES
            ${LEXXOS_NETWORK_PROGRAM_SOURCE_FILES}
    )
elseif (LEXXOS_BOARD_FAMILY_PICOCALC OR LEXXOS_BOARD STREQUAL "dev_pico2w")
    list(APPEND LEXXOS_BUS_PROGRAM_SOURCE_FILES
            lexxos/apps/picocalc/detail/psram_diagnostics_support.cpp
            lexxos/apps/network/detail/pico_network_support.cpp
            ${LEXXOS_NETWORK_PROGRAM_SOURCE_FILES}
    )
endif ()

if (LEXXOS_BOARD_FAMILY_PICOCALC)
    list(APPEND LEXXOS_BUS_PROGRAM_SOURCE_FILES
            lexxos/apps/picocalc/detail/beep_support.cpp
            lexxos/apps/picocalc/detail/picocalc_southbridge_program_support.cpp
    )
endif ()

set(LEXXOS_DEVICE_SOURCE_FILES
        lexxos/devices/audio/audio_registry.cpp
        lexxos/devices/display_device.cpp
        lexxos/devices/gamepad_device.cpp
        lexxos/devices/input_device.cpp
        lexxos/devices/pointing_device.cpp
        lexxos/devices/input/composite_gamepad.cpp
        lexxos/devices/rtc/date_time.cpp
        lexxos/devices/rtc/pcf8563.cpp
        lexxos/devices/rtc/software_rtc.cpp
        lexxos/devices/device_manager.cpp
        lexxos/composition/board_device_providers.cpp
)

if (LEXXOS_FEATURE_DS3231_RTC)
    list(APPEND LEXXOS_DEVICE_SOURCE_FILES
            lexxos/devices/rtc/ds3231.cpp
    )
endif ()

if (LEXXOS_BOARD_FAMILY_PICOCALC)
    list(APPEND LEXXOS_DEVICE_SOURCE_FILES
            lexxos/devices/picocalc/detail/picocalc_southbridge_backlight_mapper.cpp
            lexxos/devices/picocalc/detail/picocalc_southbridge_command_tracker.cpp
            lexxos/devices/picocalc/detail/picocalc_southbridge_requests.cpp
            lexxos/devices/picocalc/detail/picocalc_southbridge_snapshot_cache.cpp
            lexxos/devices/picocalc/picocalc_southbridge.cpp
            lexxos/devices/picocalc/picocalc_southbridge_resource_policy.cpp
            lexxos/devices/picocalc/picocalc_southbridge_service.cpp
    )
endif ()

if (LEXXOS_BOARD_FAMILY_CARDPUTER)
    list(APPEND LEXXOS_DEVICE_SOURCE_FILES
            lexxos/devices/espidf/cardputer_ir_transmitter.cpp
            lexxos/devices/espidf/cardputer_keyboard.cpp
            lexxos/devices/espidf/cardputer_tca8418_keyboard.cpp
            lexxos/devices/sensors/bmi270.cpp
            lexxos/devices/sensors/bmi270_config.cpp
    )
endif ()

if (LEXXOS_PLATFORM STREQUAL "espidf")
    list(APPEND LEXXOS_DEVICE_SOURCE_FILES
            lexxos/devices/audio/espidf_es8311_audio_device.cpp
            lexxos/devices/espidf/espidf_ethernet.cpp
    )
    if (LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
        list(APPEND LEXXOS_DEVICE_SOURCE_FILES
                lexxos/devices/espidf/espidf_wifi.cpp
        )
    endif ()
    if (LEXXOS_ESPIDF_ENABLE_HOSTED)
        list(APPEND LEXXOS_DEVICE_SOURCE_FILES
                lexxos/devices/espidf/espidf_hosted.cpp
        )
    endif ()

    if (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV)
        list(APPEND LEXXOS_DEVICE_SOURCE_FILES
                lexxos/devices/touch/ft5x06_touch_device.cpp
                lexxos/devices/touch/gt911_touch_device.cpp
                lexxos/devices/espidf/usb_hid_host.cpp
        )
    endif ()
endif ()

set(LEXXOS_SHELL_SOURCE_FILES
        lexxos/shell/command_registry.cpp
        lexxos/shell/shell.cpp
        lexxos/shell/shell_config.cpp
        lexxos/shell/shell_redirection_parser.cpp
        lexxos/shell/shell_escape_sequence_parser.cpp
        lexxos/shell/shell_completion.cpp
        lexxos/shell/shell_line_editor.cpp
        lexxos/shell/shell_history_policy.cpp
        lexxos/terminal/terminal.cpp
)

set(LEXXOS_VFS_SOURCE_FILES
        lexxos/vfs/vfs.cpp
        lexxos/vfs/vfs_file_handle.cpp
        lexxos/vfs/detail/vfs_mount_table.cpp
        lexxos/vfs/detail/vfs_path.cpp
        lexxos/vfs/detail/vfs_pseudo_files.cpp
        lexxos/vfs/detail/vfs_status.cpp
        lexxos/vfs/detail/vfs_tmpfs.cpp
)

set(LEXXOS_FAT_SOURCE_FILES)
if (LEXXOS_FEATURE_FATFS)
    list(APPEND LEXXOS_FAT_SOURCE_FILES
            lexxos/fs/fat/fat_volume.cpp
            lexxos/fs/fat/fatfs/fatfs_diskio.cpp
    )
endif ()

set(LEXXOS_STORAGE_SOURCE_FILES)
if (LEXXOS_FEATURE_SD_CARD)
    list(APPEND LEXXOS_STORAGE_SOURCE_FILES
            lexxos/app/sd_automount_service.cpp
            lexxos/devices/storage/sd_card_backend.cpp
            lexxos/apps/storage/detail/sd_settings_support.cpp
    )


    if (LEXXOS_FEATURE_SD_CARD_TEST_COMMANDS)
        list(APPEND LEXXOS_STORAGE_SOURCE_FILES
                lexxos/apps/storage/detail/sd_diagnostics_support.cpp
                lexxos/apps/storage/detail/sd_speed_scan_arguments.cpp
                lexxos/apps/storage/detail/sd_speed_scan_fat_probe.cpp
                lexxos/apps/storage/detail/sd_speed_scan_raw_probe.cpp
                lexxos/apps/storage/detail/sd_speed_scan_report.cpp
                lexxos/apps/storage/detail/sd_speed_scan_settings_guard.cpp
                lexxos/apps/storage/detail/sd_speed_scan_transfer.cpp
                lexxos/apps/storage/detail/storage_program_support.cpp
        )
    endif ()

    if (NOT LEXXOS_PLATFORM STREQUAL "sim")
        list(APPEND LEXXOS_STORAGE_SOURCE_FILES
                lexxos/devices/sd_card.cpp
                lexxos/devices/storage/async_block_device.cpp
        )

        if (LEXXOS_EFFECTIVE_SD_BACKEND STREQUAL "spi")
            list(APPEND LEXXOS_STORAGE_SOURCE_FILES
                    lexxos/devices/storage/sd_card_spi.cpp
                    lexxos/devices/storage/detail/sd_card_transfer_guard.cpp
                    lexxos/devices/storage/detail/sd_card_transfer_mutex.cpp
                    lexxos/devices/storage/sd_spi_block_ops.cpp
                    lexxos/devices/storage/sd_spi_protocol.cpp
                    lexxos/devices/storage/sd_spi_transport.cpp
            )
        elseif (LEXXOS_PLATFORM STREQUAL "espidf")
            list(APPEND LEXXOS_STORAGE_SOURCE_FILES
                    lexxos/devices/espidf/sd_sdio_backend_espidf.cpp
            )
        elseif (NOT LEXXOS_PLATFORM STREQUAL "stm32f4")
            list(APPEND LEXXOS_STORAGE_SOURCE_FILES
                    lexxos/devices/storage/sd_sdio_backend_stub.cpp
            )
        endif ()
    endif ()
endif ()

set(LEXXOS_DISPLAY_SOURCE_FILES)
if (LEXXOS_FEATURE_DISPLAY)
    if (LEXXOS_FEATURE_DEBUG_DISPLAY)
        list(APPEND LEXXOS_DISPLAY_SOURCE_FILES
                lexxos/app/debug_status_display.cpp
                lexxos/devices/debug_display.cpp
                lexxos/devices/display/ssd1306_display.cpp
        )
    endif ()

    if (NOT LEXXOS_PLATFORM STREQUAL "espidf")
        list(APPEND LEXXOS_DISPLAY_SOURCE_FILES
                lexxos/devices/display/st7735_display.cpp
        )
    endif ()

    if (LEXXOS_BOARD_FAMILY_PICOCALC)
        list(APPEND LEXXOS_DISPLAY_SOURCE_FILES
                lexxos/devices/display/ili9488_display.cpp
        )
    endif ()
endif ()

set(LEXXOS_INPUT_SOURCE_FILES)
if (LEXXOS_FEATURE_KEYBOARD)
    list(APPEND LEXXOS_INPUT_SOURCE_FILES
            lexxos/devices/input/keyboard_gamepad_adapter.cpp
            lexxos/devices/input/matrix_keyboard.cpp
            lexxos/devices/input/pcf8575_matrix_keyboard.cpp
            lexxos/devices/input/sega_genesis_gamepad.cpp
            lexxos/devices/io/pcf8575.cpp
    )
endif ()

if (LEXXOS_BOARD_FAMILY_PICOCALC)
    if (NOT LEXXOS_FEATURE_KEYBOARD)
        list(APPEND LEXXOS_INPUT_SOURCE_FILES
                lexxos/devices/input/keyboard_gamepad_adapter.cpp
        )
    endif ()
    list(APPEND LEXXOS_INPUT_SOURCE_FILES
            lexxos/devices/input/detail/picocalc_keyboard_controller.cpp
            lexxos/devices/input/detail/picocalc_keyboard_diagnostics.cpp
            lexxos/devices/input/detail/picocalc_keyboard_event_bridge.cpp
            lexxos/devices/input/detail/picocalc_keyboard_fifo_poller.cpp
            lexxos/devices/input/detail/picocalc_keyboard_initializer.cpp
            lexxos/devices/input/detail/picocalc_keyboard_input_state.cpp
            lexxos/devices/input/detail/picocalc_keyboard_recovery_policy.cpp
            lexxos/devices/input/detail/picocalc_keyboard_runtime_service.cpp
            lexxos/devices/input/detail/picocalc_keyboard_transport.cpp
            lexxos/devices/input/detail/picocalc_scan_code_state.cpp
            lexxos/devices/input/picocalc_keyboard_layout.cpp
            lexxos/devices/input/picocalc_keyboard.cpp
    )
endif ()

set(LEXXOS_GAMEBOY_SOURCE_FILES
        lexxos/apps/gameboy/detail/cartridge_lifetime_guard.cpp
        lexxos/apps/gameboy/detail/gameboy_display_presenter.cpp
        lexxos/apps/gameboy/detail/gameboy_emulator_runner.cpp
        lexxos/apps/gameboy/detail/gameboy_input_mapper.cpp
        lexxos/apps/gameboy/detail/gameboy_launcher.cpp
        lexxos/apps/gameboy/detail/gameboy_pcm_output_guard.cpp
        lexxos/apps/gameboy/detail/gameboy_profiler.cpp
        lexxos/apps/gameboy/detail/gameboy_program_support.cpp
        lexxos/apps/gameboy/detail/gameboy_rom_loader.cpp
        lexxos/apps/gameboy/detail/gameboy_runner.cpp
        lexxos/apps/gameboy/detail/gameboy_runtime_types.cpp
        lexxos/apps/gameboy/detail/gameboy_settings.cpp
        lexxos/apps/gameboy/detail/gameboy_test_diagnostics.cpp
        lexxos/apps/gameboy/detail/gameboy_trace.cpp
        lexxos/apps/gameboy/detail/gameboy_vram_diagnostics.cpp
        lexxos/apps/gameboy/detail/gameboy_workspace.cpp
        lexxos/gameboy/apu.cpp
        lexxos/gameboy/cartridge_header.cpp
        lexxos/gameboy/cartridge.cpp
        lexxos/gameboy/bus.cpp
        lexxos/gameboy/cpu.cpp
        lexxos/gameboy/frame_buffer.cpp
        lexxos/gameboy/ppu.cpp
)

if (LEXXOS_PLATFORM MATCHES "^(pico|stm32f4|espidf|sim)$")
    list(APPEND LEXXOS_DEVICE_SOURCE_FILES
            lexxos/composition/detail/${LEXXOS_PLATFORM}_device_providers.cpp)
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/Commands.cmake")

lexxos_make_absolute_sources(LEXXOS_RUNTIME_SOURCES ${LEXXOS_RUNTIME_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_PROGRAM_SOURCES ${LEXXOS_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_CORE_PROGRAM_SOURCES ${LEXXOS_CORE_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_EDITOR_PROGRAM_SOURCES ${LEXXOS_EDITOR_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_FILESYSTEM_PROGRAM_SOURCES ${LEXXOS_FILESYSTEM_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_DEVICE_PROGRAM_SOURCES ${LEXXOS_DEVICE_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_UTILITY_PROGRAM_SOURCES ${LEXXOS_UTILITY_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_MIDI_PROGRAM_SOURCES ${LEXXOS_MIDI_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_BUS_PROGRAM_SOURCES ${LEXXOS_BUS_PROGRAM_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_DEVICE_SOURCES ${LEXXOS_DEVICE_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_SHELL_SOURCES ${LEXXOS_SHELL_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_VFS_SOURCES ${LEXXOS_VFS_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_FAT_SOURCES ${LEXXOS_FAT_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_STORAGE_SOURCES ${LEXXOS_STORAGE_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_DISPLAY_SOURCES ${LEXXOS_DISPLAY_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_INPUT_SOURCES ${LEXXOS_INPUT_SOURCE_FILES})
lexxos_make_absolute_sources(LEXXOS_GAMEBOY_SOURCES ${LEXXOS_GAMEBOY_SOURCE_FILES})

set(LEXXOS_COMMON_SOURCES
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
    list(APPEND LEXXOS_COMMON_SOURCES ${LEXXOS_GAMEBOY_SOURCES})
endif ()
