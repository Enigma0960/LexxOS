add_executable(lexxos_contract_tests
        program_arguments_tests.cpp
        console_input_tests.cpp
        program_input_tests.cpp
        lifecycle_event_bus_tests.cpp
        runtime_profile_contract_tests.cpp
        picocalc_keyboard_layout_tests.cpp
        picocalc_southbridge_contract_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/program_input_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input/picocalc_keyboard_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/picocalc/picocalc_southbridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rtc/date_time.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
)

add_executable(lexxos_system_contract_tests
        operating_system_contract_tests.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/app_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/operating_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/runtime/capabilities.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_memory_arena.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_supervisor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_gameboy_settings_tests
        gameboy_settings_tests.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/gameboy/detail/gameboy_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/apu.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge_header.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_vfs_file_string_tests
        vfs_file_string_tests.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_lcd_console_viewport_tests
        lcd_console_viewport_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_csi_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_screen_buffer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_terminal_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_viewport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
)

add_executable(lexxos_services_contract_tests
        platform_services_contract_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_mirror.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_output.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/null_console_mirror_sink.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/program_input_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/move_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/rename_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/common/pretty_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/storage/block_devices_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/audio/audio_registry.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/gamepad_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/pointing_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge_header.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/app_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/operating_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/processor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/runtime/capabilities.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_memory_arena.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_supervisor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/command_registry.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_config.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_completion.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_escape_sequence_parser.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_line_editor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_history_policy.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_redirection_parser.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)

target_include_directories(lexxos_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_include_directories(lexxos_system_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_vfs_file_string_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_gameboy_settings_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_lcd_console_viewport_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_include_directories(lexxos_services_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_compile_features(lexxos_contract_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_system_contract_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_vfs_file_string_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_gameboy_settings_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_lcd_console_viewport_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_services_contract_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_services_contract_tests PRIVATE LEXXOS_TEST_FORCE_NULL_CONSOLE_MIRROR=1)

add_executable(lexxos_shell_history_policy_tests
        shell_history_policy_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_history_policy.cpp)
target_include_directories(lexxos_shell_history_policy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_shell_history_policy_tests PRIVATE cxx_std_20)

# Exercise guest game logic natively; assertions must stay enabled in release builds.
foreach (game IN ITEMS pacman snake tetris)
    add_executable(lexxos_${game}_tests test_${game}.cpp ${PROJECT_SOURCE_DIR}/apps/${game}/game.cpp)
    target_compile_features(lexxos_${game}_tests PRIVATE cxx_std_20)
    if (MSVC)
        target_compile_options(lexxos_${game}_tests PRIVATE /UNDEBUG)
    else ()
        target_compile_options(lexxos_${game}_tests PRIVATE -UNDEBUG)
    endif ()
endforeach ()

# Public build presets target the OS executable. CTest must build its own binaries,
# including when a single test is selected in a fresh build directory.
foreach (test_target IN ITEMS
        lexxos_contract_tests
        lexxos_system_contract_tests
        lexxos_gameboy_settings_tests
        lexxos_vfs_file_string_tests
        lexxos_lcd_console_viewport_tests
        lexxos_services_contract_tests
        lexxos_shell_history_policy_tests
        lexxos_pacman_tests
        lexxos_snake_tests
        lexxos_tetris_tests)
    add_test(NAME ${test_target}_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --config $<CONFIG> --target ${test_target})
    set_tests_properties(${test_target}_build PROPERTIES
            FIXTURES_SETUP ${test_target}_ready
            RESOURCE_LOCK lexxos_test_build)
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
            FIXTURES_REQUIRED ${test_target}_ready)
endforeach ()
