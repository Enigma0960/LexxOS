include_guard(GLOBAL)

# Shared ESP-IDF environment preparation for LexxOS and external projects.
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


# Select an installed virtual environment for this ESP-IDF major/minor version,
# rather than letting export.sh infer it from the host's default Python version.
function(lexxos_select_espidf_python_environment)
    set(LEXXOS_IDF_PYTHON_ENV_PATH "" CACHE PATH "Explicit ESP-IDF Python virtual environment")
    if (LEXXOS_IDF_PYTHON_ENV_PATH)
        set(_lexxos_python_env "${LEXXOS_IDF_PYTHON_ENV_PATH}")
    elseif (DEFINED ENV{IDF_PYTHON_ENV_PATH} AND NOT "$ENV{IDF_PYTHON_ENV_PATH}" STREQUAL "")
        set(_lexxos_python_env "$ENV{IDF_PYTHON_ENV_PATH}")
    else ()
        set(_lexxos_tools_root "$ENV{IDF_TOOLS_PATH}")
        if (NOT _lexxos_tools_root)
            set(_lexxos_tools_root "$ENV{HOME}/.espressif")
        endif ()
        set(_lexxos_version_file "${LEXXOS_IDF_PATH}/tools/cmake/version.cmake")
        if (EXISTS "${_lexxos_version_file}")
            file(READ "${_lexxos_version_file}" _lexxos_version_content)
            string(REGEX MATCH "set\\(IDF_VERSION_MAJOR[ \t]+([0-9]+)\\)" _lexxos_major_match "${_lexxos_version_content}")
            set(_lexxos_major "${CMAKE_MATCH_1}")
            string(REGEX MATCH "set\\(IDF_VERSION_MINOR[ \t]+([0-9]+)\\)" _lexxos_minor_match "${_lexxos_version_content}")
            set(_lexxos_minor "${CMAKE_MATCH_1}")
            if (_lexxos_major_match AND _lexxos_minor_match)
                file(GLOB _lexxos_python_envs LIST_DIRECTORIES true
                        "${_lexxos_tools_root}/python_env/idf${_lexxos_major}.${_lexxos_minor}_py*_env")
                list(SORT _lexxos_python_envs COMPARE NATURAL ORDER DESCENDING)
                foreach (_lexxos_candidate IN LISTS _lexxos_python_envs)
                    if (EXISTS "${_lexxos_candidate}/bin/python"
                            AND EXISTS "${_lexxos_candidate}/bin/python3")
                        set(_lexxos_python_env "${_lexxos_candidate}")
                        break()
                    endif ()
                endforeach ()
            endif ()
        endif ()
    endif ()

    if (_lexxos_python_env)
        if (NOT EXISTS "${_lexxos_python_env}/bin/python"
                OR NOT EXISTS "${_lexxos_python_env}/bin/python3")
            message(FATAL_ERROR "ESP-IDF Python environment is incomplete: ${_lexxos_python_env}")
        endif ()
        set(ENV{IDF_PYTHON_ENV_PATH} "${_lexxos_python_env}")
        message(STATUS "ESP-IDF Python environment: ${_lexxos_python_env}")
    endif ()
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
    lexxos_select_espidf_python_environment()
    # ESP-IDF 6 export.sh can return success even when activate.py fails.
    # Call the official activation helper directly when the venv is known,
    # and propagate its exit status before evaluating the exported variables.
    if (EXISTS "${LEXXOS_IDF_PATH}/tools/activate.py"
            AND EXISTS "$ENV{IDF_PYTHON_ENV_PATH}/bin/python")
        set(_lexxos_export_command
                "exports=\"$(\"$ENV{IDF_PYTHON_ENV_PATH}/bin/python\" \"${LEXXOS_IDF_PATH}/tools/activate.py\" --export --shell bash)\" && eval \"\$exports\" && /usr/bin/env")
    else ()
        set(_lexxos_export_command "source \"${export_script}\" >/dev/null && /usr/bin/env")
    endif ()
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

    if (NOT _lexxos_export_result STREQUAL "0")
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
    # The environment has already been imported. A non-login shell preserves
    # that PATH without selecting a different host Python or activating twice.
    set(_lexxos_exported_command /bin/bash -c "${shell_command}")
    execute_process(
            COMMAND ${_lexxos_exported_command}
            RESULT_VARIABLE _lexxos_exported_result
            OUTPUT_VARIABLE _lexxos_exported_stdout
            ERROR_VARIABLE _lexxos_exported_stderr
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT _lexxos_exported_result STREQUAL "0")
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

# Keep the tool list identical for the application and its bootloader.
set(_LEXXOS_ESPIDF_BINUTILS AR RANLIB NM OBJDUMP OBJCOPY READELF STRIP LINKER SIZE ADDR2LINE)

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
    # CMake may retain native tools in the cache when an IDE configures a
    # cross build. Resolve binutils from the selected compiler directory only.
    foreach (_lexxos_tool IN LISTS _LEXXOS_ESPIDF_BINUTILS)
        string(TOLOWER "${_lexxos_tool}" _lexxos_tool_name)
        if (_lexxos_tool STREQUAL "LINKER")
            set(_lexxos_tool_name ld)
        endif ()
        set(_lexxos_tool_path
                "${_lexxos_espidf_toolchain_bin_dir}/${_lexxos_toolchain_prefix}-${_lexxos_tool_name}")
        if (NOT EXISTS "${_lexxos_tool_path}")
            message(FATAL_ERROR "ESP-IDF tool not found: ${_lexxos_tool_path}")
        endif ()
        set(CMAKE_${_lexxos_tool} "${_lexxos_tool_path}" CACHE FILEPATH "ESP-IDF target tool" FORCE)
        set(CMAKE_${_lexxos_tool} "${_lexxos_tool_path}" PARENT_SCOPE)
    endforeach ()

    foreach (_lexxos_language IN ITEMS C CXX)
        foreach (_lexxos_tool IN ITEMS AR RANLIB)
            string(TOLOWER "${_lexxos_tool}" _lexxos_tool_name)
            set(_lexxos_tool_path
                    "${_lexxos_espidf_toolchain_bin_dir}/${_lexxos_toolchain_prefix}-gcc-${_lexxos_tool_name}")
            if (EXISTS "${_lexxos_tool_path}")
                set(CMAKE_${_lexxos_language}_COMPILER_${_lexxos_tool}
                        "${_lexxos_tool_path}" CACHE FILEPATH "ESP-IDF GCC target tool" FORCE)
                set(CMAKE_${_lexxos_language}_COMPILER_${_lexxos_tool} "${_lexxos_tool_path}" PARENT_SCOPE)
            endif ()
        endforeach ()
    endforeach ()

    list(PREPEND CMAKE_PROGRAM_PATH "${_lexxos_espidf_toolchain_bin_dir}")
    list(REMOVE_DUPLICATES CMAKE_PROGRAM_PATH)
    set(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH}" CACHE STRING "Program search paths" FORCE)
    set(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH}" PARENT_SCOPE)
    message(STATUS "ESP-IDF binutils: ${_lexxos_espidf_toolchain_bin_dir}/${_lexxos_toolchain_prefix}-*")

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
    # Keep IDF's own toolchain file (including response-file handling).
    # Provide absolute tools as well as the compiler search path.
    if (LEXXOS_ESPIDF_TOOLCHAIN_BIN_DIR)
        lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_PROGRAM_PATH:PATH=${LEXXOS_ESPIDF_TOOLCHAIN_BIN_DIR}")
        foreach (_lexxos_tool IN ITEMS C_COMPILER CXX_COMPILER ASM_COMPILER
                ${_LEXXOS_ESPIDF_BINUTILS}
                C_COMPILER_AR C_COMPILER_RANLIB CXX_COMPILER_AR CXX_COMPILER_RANLIB)
            if (CMAKE_${_lexxos_tool})
                lexxos_append_espidf_subproject_cmake_arg("-DCMAKE_${_lexxos_tool}:FILEPATH=${CMAKE_${_lexxos_tool}}")
            endif ()
        endforeach ()
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
