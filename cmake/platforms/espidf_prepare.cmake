include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)


set(LEXXOS_IDF_PATH "" CACHE PATH "ESP-IDF root directory")
if (LEXXOS_IDF_PATH STREQUAL "")
    if (DEFINED IDF_PATH AND NOT IDF_PATH STREQUAL "")
        set(LEXXOS_IDF_PATH "${IDF_PATH}" CACHE PATH "ESP-IDF root directory" FORCE)
    elseif (DEFINED ENV{LEXXOS_IDF_PATH} AND NOT "$ENV{LEXXOS_IDF_PATH}" STREQUAL "")
        set(LEXXOS_IDF_PATH "$ENV{LEXXOS_IDF_PATH}" CACHE PATH "ESP-IDF root directory" FORCE)
    elseif (DEFINED ENV{IDF_PATH} AND NOT "$ENV{IDF_PATH}" STREQUAL "")
        set(LEXXOS_IDF_PATH "$ENV{IDF_PATH}" CACHE PATH "ESP-IDF root directory" FORCE)
    elseif (DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/.espressif/v6.0.2/esp-idf/export.sh")
        set(LEXXOS_IDF_PATH "$ENV{HOME}/.espressif/v6.0.2/esp-idf" CACHE PATH "ESP-IDF root directory" FORCE)
    endif ()
endif ()

if (LEXXOS_IDF_PATH STREQUAL "")
    message(FATAL_ERROR
            "ESP-IDF path is not set. Set IDF_PATH/LEXXOS_IDF_PATH, pass -DLEXXOS_IDF_PATH=/path/to/esp-idf, "
            "or load ESP-IDF export.sh before configuring LEXXOS_PLATFORM=espidf.")
endif ()

if (NOT EXISTS "${LEXXOS_IDF_PATH}/tools/cmake/project.cmake")
    message(FATAL_ERROR "ESP-IDF project.cmake was not found: ${LEXXOS_IDF_PATH}/tools/cmake/project.cmake")
endif ()

set(ENV{IDF_PATH} "${LEXXOS_IDF_PATH}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LEXXOS_ESPIDF_COMPONENT_DIR "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos")
if (NOT EXISTS "${LEXXOS_ESPIDF_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "ESP-IDF LexxOS component is missing: ${LEXXOS_ESPIDF_COMPONENT_DIR}/CMakeLists.txt")
endif ()

set(EXTRA_COMPONENT_DIRS
        "${LEXXOS_ESPIDF_COMPONENT_DIR}"
        CACHE STRING "Additional ESP-IDF components used by LexxOS" FORCE
)

# ESP-IDF performs an early component-requirements configure pass before the
# normal LexxOS project configure state is fully visible.  Keep the selected
# board in a dedicated cache/environment hint so the component CMakeLists does
# not fall back from m5stack_cardputer_adv to the generic ESP32-S3 Cardputer
# profile during that early pass.
set(LEXXOS_ESPIDF_BOARD "${LEXXOS_BOARD}" CACHE STRING "LexxOS board profile selected for ESP-IDF component discovery" FORCE)
set(ENV{LEXXOS_ESPIDF_BOARD} "${LEXXOS_BOARD}")
set(ENV{LEXXOS_BOARD} "${LEXXOS_BOARD}")

if (LEXXOS_BOARD STREQUAL "m5stack_cardputer" OR LEXXOS_BOARD STREQUAL "m5stack_cardputer_adv")
    set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.cardputer.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_m5stack_cardputer_8mb.csv")
elseif (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    set(IDF_TARGET "esp32p4" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.jc_esp32p4_m3_dev.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_jc_esp32p4_m3_dev_16mb.csv")
else ()
    set(IDF_TARGET "esp32" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_m5stack_basic_v1_4mb.csv")
endif ()

if (NOT EXISTS "${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}")
    message(FATAL_ERROR "ESP-IDF sdkconfig defaults are missing: ${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}")
endif ()

if (NOT EXISTS "${LEXXOS_ESPIDF_PARTITION_TABLE}")
    message(FATAL_ERROR "ESP-IDF partition table is missing: ${LEXXOS_ESPIDF_PARTITION_TABLE}")
endif ()

set(SDKCONFIG_DEFAULTS
        "${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}"
        CACHE STRING "ESP-IDF sdkconfig defaults for LexxOS" FORCE
)
set(PARTITION_TABLE_CUSTOM_FILENAME
        "${LEXXOS_ESPIDF_PARTITION_TABLE}"
        CACHE STRING "ESP-IDF custom partition table for LexxOS" FORCE
)

# The root CMake entry point must not create or reuse sdkconfig in the repository
# root. Keep ESP-IDF configuration inside the selected build directory, so the
# repository can host multiple platform profiles without source-tree side effects.
set(SDKCONFIG
        "${CMAKE_BINARY_DIR}/sdkconfig"
        CACHE FILEPATH "ESP-IDF sdkconfig generated for this LexxOS build directory" FORCE
)

set(PROJECT_VER "${LEXXOS_VERSION}")

set(LEXXOS_ESPIDF_EXPORT_SHELL_ARCH "" CACHE STRING
        "Optional macOS architecture used to run ESP-IDF export.sh helper commands; empty selects native arm64 on Apple Silicon")

function(lexxos_detect_espidf_export_shell_arch output_variable)
    set(_lexxos_export_shell_arch "${LEXXOS_ESPIDF_EXPORT_SHELL_ARCH}")

    if (_lexxos_export_shell_arch STREQUAL ""
            AND EXISTS "/usr/bin/arch"
            AND EXISTS "/usr/bin/uname")
        execute_process(
                COMMAND /usr/bin/uname -s
                OUTPUT_VARIABLE _lexxos_uname_system
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if (_lexxos_uname_system STREQUAL "Darwin")
            execute_process(
                    COMMAND /usr/bin/uname -m
                    OUTPUT_VARIABLE _lexxos_uname_machine
                    OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            if (_lexxos_uname_machine STREQUAL "arm64")
                set(_lexxos_export_shell_arch "arm64")
            elseif (EXISTS "/usr/sbin/sysctl")
                execute_process(
                        COMMAND /usr/sbin/sysctl -n hw.optional.arm64
                        OUTPUT_VARIABLE _lexxos_hw_optional_arm64
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                if (_lexxos_hw_optional_arm64 STREQUAL "1")
                    set(_lexxos_export_shell_arch "arm64")
                endif ()
            endif ()
        endif ()
    endif ()

    set(${output_variable} "${_lexxos_export_shell_arch}" PARENT_SCOPE)
endfunction()

function(lexxos_make_espidf_export_shell_command output_variable shell_command)
    set(_lexxos_shell_command /bin/bash -lc "${shell_command}")

    lexxos_detect_espidf_export_shell_arch(_lexxos_export_shell_arch)
    if (NOT _lexxos_export_shell_arch STREQUAL "" AND EXISTS "/usr/bin/arch")
        set(_lexxos_shell_command
                /usr/bin/arch "-${_lexxos_export_shell_arch}" /bin/bash -lc "${shell_command}")
    endif ()

    set(${output_variable} ${_lexxos_shell_command} PARENT_SCOPE)
endfunction()

function(lexxos_wrap_espidf_python_for_export_arch input_python output_variable)
    set(_lexxos_wrapped_python "${input_python}")

    lexxos_detect_espidf_export_shell_arch(_lexxos_export_shell_arch)
    if (NOT _lexxos_export_shell_arch STREQUAL "" AND EXISTS "/usr/bin/arch")
        set(_lexxos_python_wrapper_dir "${CMAKE_BINARY_DIR}/generated/tools")
        set(_lexxos_python_wrapper "${_lexxos_python_wrapper_dir}/espidf-python-${_lexxos_export_shell_arch}")

        file(MAKE_DIRECTORY "${_lexxos_python_wrapper_dir}")
        file(WRITE "${_lexxos_python_wrapper}"
                "#!/bin/sh\n"
                "exec /usr/bin/arch -${_lexxos_export_shell_arch} \"${input_python}\" \"$@\"\n")
        file(CHMOD "${_lexxos_python_wrapper}"
                PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE)

        set(_lexxos_wrapped_python "${_lexxos_python_wrapper}")
        set(LEXXOS_ESPIDF_REAL_PYTHON_EXECUTABLE "${input_python}" CACHE FILEPATH
                "Real ESP-IDF Python interpreter wrapped by LexxOS for host architecture selection"
                FORCE)
    endif ()

    set(${output_variable} "${_lexxos_wrapped_python}" PARENT_SCOPE)
endfunction()


function(lexxos_import_espidf_export_environment export_script)
    if (NOT EXISTS "${export_script}")
        message(FATAL_ERROR "ESP-IDF export.sh is missing: ${export_script}")
    endif ()

    # CLion and other GUI launchers often start CMake without the shell
    # environment produced by ESP-IDF export.sh. IDF's project.cmake may still
    # resolve compiler names such as xtensa-esp32s3-elf-gcc, but CMake cannot
    # find them unless the exported toolchain directories are present in PATH.
    # Import only the small set of variables that ESP-IDF tools need instead of
    # replacing the entire CMake process environment.
    set(_lexxos_export_command "source \"${export_script}\" >/dev/null && /usr/bin/env")
    lexxos_make_espidf_export_shell_command(
            _lexxos_export_shell_command
            "${_lexxos_export_command}"
    )
    execute_process(
            COMMAND ${_lexxos_export_shell_command}
            RESULT_VARIABLE _lexxos_export_result
            OUTPUT_VARIABLE _lexxos_export_stdout
            ERROR_VARIABLE _lexxos_export_stderr
    )

    if (NOT _lexxos_export_result EQUAL 0)
        message(FATAL_ERROR
                "Failed to import ESP-IDF environment from ${export_script}\n"
                "${_lexxos_export_stderr}")
    endif ()

    set(_lexxos_espidf_env_names
            PATH
            IDF_PATH
            LEXXOS_IDF_PATH
            IDF_TOOLS_PATH
            IDF_PYTHON_ENV_PATH
            OPENOCD_SCRIPTS
            PYTHONPATH
            ESP_ROM_ELF_DIR
    )

    string(REPLACE "\n" ";" _lexxos_export_lines "${_lexxos_export_stdout}")
    foreach (_lexxos_export_line IN LISTS _lexxos_export_lines)
        if (_lexxos_export_line MATCHES "^([^=]+)=(.*)$")
            set(_lexxos_env_name "${CMAKE_MATCH_1}")
            set(_lexxos_env_value "${CMAKE_MATCH_2}")
            list(FIND _lexxos_espidf_env_names "${_lexxos_env_name}" _lexxos_env_index)
            if (NOT _lexxos_env_index EQUAL -1)
                set(ENV{${_lexxos_env_name}} "${_lexxos_env_value}")
            endif ()
        endif ()
    endforeach ()

    message(STATUS "ESP-IDF environment imported from ${export_script}")
endfunction()

function(lexxos_run_espidf_exported_shell output_variable shell_command)
    set(_lexxos_export_script "$ENV{IDF_PATH}/export.sh")
    if (NOT EXISTS "${_lexxos_export_script}")
        message(FATAL_ERROR "ESP-IDF export.sh is missing: ${_lexxos_export_script}")
    endif ()

    # Do not rely on CMake's find_program() observing PATH changes made with
    # set(ENV{PATH} ...). Query the same shell environment that export.sh
    # creates and store absolute paths in the CMake cache before project().
    set(_lexxos_exported_command "source \"${_lexxos_export_script}\" >/dev/null && ${shell_command}")
    lexxos_make_espidf_export_shell_command(
            _lexxos_export_shell_command
            "${_lexxos_exported_command}"
    )
    execute_process(
            COMMAND ${_lexxos_export_shell_command}
            RESULT_VARIABLE _lexxos_exported_result
            OUTPUT_VARIABLE _lexxos_exported_stdout
            ERROR_VARIABLE _lexxos_exported_stderr
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT _lexxos_exported_result EQUAL 0)
        message(FATAL_ERROR
                "Failed to query ESP-IDF exported environment with command: ${shell_command}\n"
                "${_lexxos_exported_stderr}")
    endif ()

    set(${output_variable} "${_lexxos_exported_stdout}" PARENT_SCOPE)
endfunction()

function(lexxos_configure_espidf_python)
    set(_lexxos_python_candidates)

    if (DEFINED ENV{IDF_PYTHON_ENV_PATH} AND NOT "$ENV{IDF_PYTHON_ENV_PATH}" STREQUAL "")
        list(APPEND _lexxos_python_candidates
                "$ENV{IDF_PYTHON_ENV_PATH}/bin/python3"
                "$ENV{IDF_PYTHON_ENV_PATH}/bin/python"
        )
    endif ()

    lexxos_run_espidf_exported_shell(
            _lexxos_exported_python_env_path
            "printf '%s' \"\$IDF_PYTHON_ENV_PATH\""
    )
    if (NOT _lexxos_exported_python_env_path STREQUAL "")
        list(APPEND _lexxos_python_candidates
                "${_lexxos_exported_python_env_path}/bin/python3"
                "${_lexxos_exported_python_env_path}/bin/python"
        )
    endif ()

    foreach (_lexxos_python_candidate IN LISTS _lexxos_python_candidates)
        if (EXISTS "${_lexxos_python_candidate}")
            set(_lexxos_espidf_python "${_lexxos_python_candidate}")
            break()
        endif ()
    endforeach ()

    if (NOT _lexxos_espidf_python)
        lexxos_run_espidf_exported_shell(
                _lexxos_espidf_python
                "command -v python || command -v python3"
        )
    endif ()

    if (NOT _lexxos_espidf_python)
        message(FATAL_ERROR
                "ESP-IDF Python interpreter was not found after importing export.sh. "
                "Run ${LEXXOS_IDF_PATH}/install.sh ${IDF_TARGET} once and configure again.")
    endif ()

    lexxos_wrap_espidf_python_for_export_arch("${_lexxos_espidf_python}" _lexxos_espidf_python)

    set(PYTHON "${_lexxos_espidf_python}" CACHE FILEPATH "Python interpreter used by ESP-IDF build rules" FORCE)
    set(Python3_EXECUTABLE "${_lexxos_espidf_python}" CACHE FILEPATH "Python 3 interpreter used by ESP-IDF" FORCE)
    set(ENV{PYTHON} "${_lexxos_espidf_python}")
    message(STATUS "ESP-IDF Python: ${_lexxos_espidf_python}")
endfunction()

function(lexxos_configure_espidf_toolchain)
    if (IDF_TARGET STREQUAL "esp32s3")
        set(_lexxos_toolchain_prefix "xtensa-esp32s3-elf")
    elseif (IDF_TARGET STREQUAL "esp32")
        set(_lexxos_toolchain_prefix "xtensa-esp32-elf")
    elseif (IDF_TARGET STREQUAL "esp32p4")
        set(_lexxos_toolchain_prefix "riscv32-esp-elf")
    else ()
        return()
    endif ()

    lexxos_run_espidf_exported_shell(
            _lexxos_espidf_c_compiler
            "command -v ${_lexxos_toolchain_prefix}-gcc"
    )
    lexxos_run_espidf_exported_shell(
            _lexxos_espidf_cxx_compiler
            "command -v ${_lexxos_toolchain_prefix}-g++"
    )

    if (NOT _lexxos_espidf_c_compiler OR NOT _lexxos_espidf_cxx_compiler)
        message(FATAL_ERROR
                "ESP-IDF ${IDF_TARGET} compiler was not found after importing export.sh. "
                "Run ${LEXXOS_IDF_PATH}/install.sh ${IDF_TARGET} once and configure again.")
    endif ()

    get_filename_component(
            _lexxos_espidf_toolchain_bin_dir
            "${_lexxos_espidf_c_compiler}"
            DIRECTORY
    )

    # CLion can invoke later build targets (flash/app-flash) without the shell
    # environment that was active during configure. Store absolute compiler
    # paths in the CMake cache so the generated Ninja rules remain independent
    # of the IDE process PATH.
    set(CMAKE_C_COMPILER "${_lexxos_espidf_c_compiler}" CACHE FILEPATH "ESP-IDF C compiler" FORCE)
    set(CMAKE_CXX_COMPILER "${_lexxos_espidf_cxx_compiler}" CACHE FILEPATH "ESP-IDF C++ compiler" FORCE)
    set(CMAKE_ASM_COMPILER "${_lexxos_espidf_c_compiler}" CACHE FILEPATH "ESP-IDF ASM compiler" FORCE)
    set(ENV{CC} "${_lexxos_espidf_c_compiler}")
    set(ENV{CXX} "${_lexxos_espidf_cxx_compiler}")
    set(ENV{ASM} "${_lexxos_espidf_c_compiler}")
    set(LEXXOS_ESPIDF_TOOLCHAIN_BIN_DIR
            "${_lexxos_espidf_toolchain_bin_dir}"
            CACHE INTERNAL
            "ESP-IDF target toolchain binary directory")


    message(STATUS "ESP-IDF C compiler: ${_lexxos_espidf_c_compiler}")
    message(STATUS "ESP-IDF CXX compiler: ${_lexxos_espidf_cxx_compiler}")
endfunction()

function(lexxos_append_espidf_subproject_cmake_arg argument)
    if (COMMAND idf_build_set_property)
        idf_build_set_property(EXTRA_CMAKE_ARGS "${argument}" APPEND)
    endif ()
endfunction()


function(lexxos_configure_espidf_subproject_toolchain)
    if (NOT COMMAND idf_build_set_property)
        return()
    endif ()

    # ESP-IDF configures the bootloader as a separate CMake subproject during
    # the build step. CLion starts `cmake --build` from its own GUI environment,
    # so the PATH imported from export.sh during the top-level configure step is
    # not available to the bootloader configure command.
    # Do not pass CMAKE_<LANG>_COMPILER to the bootloader. ESP-IDF's chip
    # toolchain file intentionally initializes these variables from the
    # xtensa/riscv prefix and also prepares its response-file based cflags and
    # ldflags. Forcing compiler cache entries from the parent project can make
    # the bootloader link against the wrong implicit runtime pieces. Instead,
    # pass only the toolchain bin directory so CMake can resolve the short
    # compiler names selected by ESP-IDF's own toolchain file.
    if (CMAKE_C_COMPILER)
        get_filename_component(_lexxos_espidf_toolchain_bin_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    endif ()

    if (_lexxos_espidf_toolchain_bin_dir)
        lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_PROGRAM_PATH:PATH=${_lexxos_espidf_toolchain_bin_dir}")
    endif ()

    lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}")
    lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}")
    lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_ASM_COMPILER:FILEPATH=${CMAKE_ASM_COMPILER}")

    if (LEXXOS_ESPIDF_TOOLCHAIN_BIN_DIR)
        lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_PROGRAM_PATH:PATH=${LEXXOS_ESPIDF_TOOLCHAIN_BIN_DIR}")
    endif ()

    if (PYTHON)
        lexxos_append_espidf_subproject_cmake_arg("-DPYTHON:FILEPATH=${PYTHON}")
    endif ()

    if (Python3_EXECUTABLE)
        lexxos_append_espidf_subproject_cmake_arg("-DPython3_EXECUTABLE:FILEPATH=${Python3_EXECUTABLE}")
    endif ()

    if (CMAKE_MAKE_PROGRAM)
        lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}")
    endif ()

    message(STATUS "ESP-IDF subproject toolchain: compiler search path will be passed to bootloader")
endfunction()


lexxos_import_espidf_export_environment("$ENV{IDF_PATH}/export.sh")
lexxos_configure_espidf_python()
lexxos_configure_espidf_toolchain()

# Keep independent dependency resolutions for Xtensa and ESP32-P4 targets.
# A single root lock is otherwise rewritten on every board switch.
file(MAKE_DIRECTORY "${LEXXOS_ROOT_DIR}/ports/espidf/locks")
set(DEPENDENCIES_LOCK "${LEXXOS_ROOT_DIR}/ports/espidf/locks/${IDF_TARGET}.lock")
include("$ENV{IDF_PATH}/tools/cmake/project.cmake")
idf_build_set_property(DEPENDENCIES_LOCK "${DEPENDENCIES_LOCK}")
if (COMMAND idf_build_set_property AND NOT PYTHON STREQUAL "")
    idf_build_set_property(PYTHON "${PYTHON}")
endif ()

lexxos_configure_espidf_subproject_toolchain()
