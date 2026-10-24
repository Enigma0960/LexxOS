include_guard(GLOBAL)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(LEXXOS_COMMAND_OUTPUT "${LEXXOS_GENERATED_INCLUDE_DIR}/commands")
set(command_arguments)
foreach(feature IN LISTS LEXXOS_FEATURES)
    if (LEXXOS_FEATURE_${feature})
        list(APPEND command_arguments --enabled ${feature})
    endif()
endforeach()
foreach(group IN ITEMS SYSTEM UTILITIES ADMIN DIAGNOSTICS APPLICATIONS TESTS EXPERIMENTAL)
    if (LEXXOS_COMMANDS_${group})
        list(APPEND command_arguments --enabled GROUP_${group})
    endif()
endforeach()
if (LEXXOS_ESPIDF_ENABLE_HOSTED)
    list(APPEND command_arguments --enabled HOSTED)
endif()
if (TARGET pico_cyw43_arch_lwip_threadsafe_background)
    list(APPEND command_arguments --enabled CYW43)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${LEXXOS_ROOT_DIR}/cmake/commands.json"
        "${LEXXOS_ROOT_DIR}/tools/generate_command_catalog.py")
execute_process(COMMAND ${Python3_EXECUTABLE}
        "${LEXXOS_ROOT_DIR}/tools/generate_command_catalog.py"
        --manifest "${LEXXOS_ROOT_DIR}/cmake/commands.json"
        --output "${LEXXOS_COMMAND_OUTPUT}"
        --board "${LEXXOS_BOARD}" --platform "${LEXXOS_PLATFORM}"
        --flavor "${LEXXOS_BUILD_FLAVOR}" --backend "${LEXXOS_EFFECTIVE_SD_BACKEND}"
        ${command_arguments}
        RESULT_VARIABLE command_result ERROR_VARIABLE command_error)
if (NOT command_result EQUAL 0)
    message(FATAL_ERROR "Command catalog generation failed: ${command_error}")
endif()
include("${LEXXOS_COMMAND_OUTPUT}/command_sources.cmake")

# The manifest owns command translation units and their private helpers.
# Drivers and services remain in the feature-controlled common source lists.
list(APPEND LEXXOS_PROGRAM_SOURCE_FILES ${LEXXOS_SELECTED_COMMAND_SOURCES})

# Game Boy has no OS service callers. Keep its core only when a selected command
# needs it, and keep the interactive runtime only for gbemu.
if (NOT LEXXOS_COMMAND_HAS_GAMEBOY)
    set(LEXXOS_GAMEBOY_SOURCE_FILES)
elseif (NOT LEXXOS_COMMAND_HAS_GBEMU)
    list(REMOVE_ITEM LEXXOS_GAMEBOY_SOURCE_FILES
            lexxos/apps/gameboy/detail/gameboy_emulator_runner.cpp
            lexxos/apps/gameboy/detail/gameboy_launcher.cpp
            lexxos/apps/gameboy/detail/gameboy_pcm_output_guard.cpp)
endif()
