include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)

if ((LEXXOS_PLATFORM STREQUAL "host" OR LEXXOS_PLATFORM STREQUAL "sim") AND LEXXOS_HOST_NATIVE_ARCHITECTURE)
    execute_process(
            COMMAND uname -s
            RESULT_VARIABLE LEXXOS_UNAME_SYSTEM_RESULT
            OUTPUT_VARIABLE LEXXOS_UNAME_SYSTEM
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    if (LEXXOS_UNAME_SYSTEM_RESULT EQUAL 0 AND LEXXOS_UNAME_SYSTEM STREQUAL "Darwin")
        if (NOT LEXXOS_HOST_TARGET_ARCHITECTURE STREQUAL "")
            set(LEXXOS_NATIVE_OSX_ARCH "${LEXXOS_HOST_TARGET_ARCHITECTURE}")
        else ()
            execute_process(
                    COMMAND /usr/bin/arch -arm64 /usr/bin/true
                    RESULT_VARIABLE LEXXOS_DARWIN_ARM64_EXEC_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET
            )
            if (LEXXOS_DARWIN_ARM64_EXEC_RESULT EQUAL 0)
                set(LEXXOS_NATIVE_OSX_ARCH "arm64")
            else ()
                execute_process(
                        COMMAND python3 -c "import platform; print(platform.machine())"
                        RESULT_VARIABLE LEXXOS_PYTHON_ARCH_RESULT
                        OUTPUT_VARIABLE LEXXOS_NATIVE_OSX_ARCH
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET
                )
                if (NOT LEXXOS_PYTHON_ARCH_RESULT EQUAL 0 OR LEXXOS_NATIVE_OSX_ARCH STREQUAL "")
                    execute_process(
                            COMMAND uname -m
                            RESULT_VARIABLE LEXXOS_UNAME_ARCH_RESULT
                            OUTPUT_VARIABLE LEXXOS_NATIVE_OSX_ARCH
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET
                    )
                endif ()
            endif ()
        endif ()
        if (NOT LEXXOS_NATIVE_OSX_ARCH STREQUAL "")
            set(CMAKE_OSX_ARCHITECTURES "${LEXXOS_NATIVE_OSX_ARCH}" CACHE STRING "macOS architecture for host/sim targets" FORCE)
        endif ()
    endif ()
endif ()

if (LEXXOS_PLATFORM STREQUAL "pico")
    set(PICO_PICOTOOL_DIR "" CACHE PATH "Optional path to installed picotool CMake package directory")
    if (PICO_PICOTOOL_DIR STREQUAL "" AND DEFINED ENV{picotool_DIR} AND NOT "$ENV{picotool_DIR}" STREQUAL "")
        set(PICO_PICOTOOL_DIR "$ENV{picotool_DIR}" CACHE PATH "Optional path to installed picotool CMake package directory" FORCE)
    endif ()
    if (PICO_PICOTOOL_DIR)
        set(picotool_DIR "${PICO_PICOTOOL_DIR}" CACHE PATH "picotool CMake package directory" FORCE)
    endif ()

    if (NOT EXISTS "${LEXXOS_ROOT_DIR}/cmake/pico_sdk_import.cmake")
        message(FATAL_ERROR "pico_sdk_import.cmake is missing. Copy it from the Raspberry Pi Pico SDK into cmake/ or add it as a generated project file.")
    endif ()

    include("${LEXXOS_ROOT_DIR}/cmake/pico_sdk_import.cmake")
endif ()
