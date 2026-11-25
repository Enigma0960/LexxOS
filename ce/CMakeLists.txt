cmake_minimum_required(VERSION 3.20)
project(LexxCEPortable LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include("../config/cmake/lexxos_core_sources.cmake")
add_library(lexxos_ce STATIC ${LEXXOS_CORE_SOURCES})
target_include_directories(lexxos_ce PUBLIC "../src")
target_compile_definitions(lexxos_ce PUBLIC LEXXOS_EDITION_CE=1)
target_compile_features(lexxos_ce PUBLIC cxx_std_20)
target_compile_options(lexxos_ce PRIVATE -fno-exceptions -fno-rtti)
add_library(lexxos_ce_cooperative STATIC
        "../src/lexxos/kernel/cooperative/scheduler_cooperative.cpp"
        "../src/lexxos/kernel/cooperative/synchronization_cooperative.cpp"
        "../src/lexxos/kernel/cooperative/event_cooperative.cpp"
        "../src/lexxos/kernel/cooperative/mutex_cooperative.cpp"
        "../src/lexxos/kernel/cooperative/semaphore_cooperative.cpp")
target_link_libraries(lexxos_ce_cooperative PUBLIC lexxos_ce)
target_compile_options(lexxos_ce_cooperative PRIVATE -fno-exceptions -fno-rtti)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_filename_component(LEXXOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    add_custom_target(ce-sdk ALL
            COMMAND "${Python3_EXECUTABLE}" "${LEXXOS_ROOT}/tools/export_ce_sdk.py"
                    --root "${LEXXOS_ROOT}" --build "${CMAKE_CURRENT_BINARY_DIR}"
                    --archive "$<TARGET_FILE:lexxos_ce>"
                    --cooperative-archive "$<TARGET_FILE:lexxos_ce_cooperative>"
                    --compiler "${CMAKE_CXX_COMPILER}" --output "${CMAKE_CURRENT_BINARY_DIR}/sdk"
            DEPENDS lexxos_ce lexxos_ce_cooperative VERBATIM)
endif ()
