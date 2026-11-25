# Test package configuration with an isolated ESP-IDF environment.
file(REMOVE_RECURSE "${LEXXOS_TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${LEXXOS_TEST_OUTPUT_DIR}/idf/tools/cmake" "${LEXXOS_TEST_OUTPUT_DIR}/tools/bin" "${LEXXOS_TEST_OUTPUT_DIR}/python/bin" "${LEXXOS_TEST_OUTPUT_DIR}/project")
set(_idf "${LEXXOS_TEST_OUTPUT_DIR}/idf")
set(_tools "${LEXXOS_TEST_OUTPUT_DIR}/tools/bin")
set(_python "${LEXXOS_TEST_OUTPUT_DIR}/python_env/idf6.0_py3.14_env")
file(MAKE_DIRECTORY "${_python}/bin")
file(WRITE "${_idf}/tools/cmake/version.cmake" "set(IDF_VERSION_MAJOR 6)\nset(IDF_VERSION_MINOR 0)\n")
file(WRITE "${_idf}/tools/activate.py" "# Mock activation entry point\n")
file(WRITE "${LEXXOS_TEST_OUTPUT_DIR}/idf/export.sh"
        "export PATH=\"${_tools}:\$PATH\"\nexport IDF_PYTHON_ENV_PATH=\"${_python}\"\n")
foreach (_tool gcc g++ ar ranlib nm objdump objcopy readelf strip ld size addr2line gcc-ar gcc-ranlib)
    set(_compiler "xtensa-esp32-elf-${_tool}")
    file(WRITE "${LEXXOS_TEST_OUTPUT_DIR}/tools/bin/${_compiler}" "#!/bin/sh\nexit 0\n")
    file(CHMOD "${LEXXOS_TEST_OUTPUT_DIR}/tools/bin/${_compiler}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endforeach ()
foreach (_name python python3)
    file(WRITE "${_python}/bin/${_name}"
            "#!/bin/sh\n[ \"\$IDF_PYTHON_ENV_PATH\" = \"${_python}\" ] || exit 1\nprintf '%s\\n' 'export PATH=\"${_tools}:\$PATH\"' 'export IDF_PYTHON_ENV_PATH=\"${_python}\"'\n")
    file(CHMOD "${_python}/bin/${_name}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endforeach ()
file(WRITE "${_idf}/export.sh" "return 1\n")

file(WRITE "${LEXXOS_TEST_OUTPUT_DIR}/idf/tools/cmake/project.cmake" [=[
set(IDF_TARGET esp32 CACHE STRING "Mock target")
function(idf_build_process)
endfunction()
function(idf_build_set_property property value)
    set_property(GLOBAL APPEND PROPERTY "MOCK_${property}" "${value}")
endfunction()
]=])

file(WRITE "${LEXXOS_TEST_OUTPUT_DIR}/project/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.20)
find_package(${TEST_PACKAGE} CONFIG REQUIRED)
if (NOT CMAKE_C_COMPILER STREQUAL "${TEST_TOOLS}/xtensa-esp32-elf-gcc"
        OR NOT CMAKE_CXX_COMPILER STREQUAL "${TEST_TOOLS}/xtensa-esp32-elf-g++"
        OR NOT CMAKE_ASM_COMPILER STREQUAL CMAKE_C_COMPILER)
    message(FATAL_ERROR "ESP-IDF compilers were not configured")
endif ()
if (NOT EXISTS "$ENV{IDF_PYTHON_ENV_PATH}/bin/python3" OR NOT EXISTS "${PYTHON}")
    message(FATAL_ERROR "ESP-IDF Python was not configured")
endif ()
foreach (_tool IN ITEMS AR RANLIB NM OBJDUMP OBJCOPY READELF STRIP LINKER SIZE ADDR2LINE
        C_COMPILER_AR C_COMPILER_RANLIB CXX_COMPILER_AR CXX_COMPILER_RANLIB)
    if (NOT CMAKE_${_tool} MATCHES "^${TEST_TOOLS}/xtensa-esp32-elf-")
        message(FATAL_ERROR "Host tool selected: CMAKE_${_tool}=${CMAKE_${_tool}}")
    endif ()
endforeach ()
get_property(_args GLOBAL PROPERTY MOCK_EXTRA_CMAKE_ARGS)
foreach (_tool IN ITEMS AR RANLIB NM OBJDUMP OBJCOPY READELF STRIP LINKER SIZE ADDR2LINE)
    if (NOT "-DCMAKE_${_tool}:FILEPATH=${CMAKE_${_tool}}" IN_LIST _args)
        message(FATAL_ERROR "Bootloader tool missing: ${_tool}")
    endif ()
endforeach ()
if (NOT "-DCMAKE_PROGRAM_PATH:PATH=${TEST_TOOLS}" IN_LIST _args)
    message(FATAL_ERROR "ESP-IDF bootloader toolchain was not configured")
endif ()
project(ExternalFirmware LANGUAGES NONE)
]=])

foreach (_package LexxIDF LexxCE LexxSDK)
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env --unset=IDF_PATH --unset=LEXXOS_IDF_PATH
                    --unset=LEXXOS_PLATFORM --unset=IDF_TARGET --unset=IDF_PYTHON_ENV_PATH
                    "IDF_TOOLS_PATH=${LEXXOS_TEST_OUTPUT_DIR}"
                    "${CMAKE_COMMAND}" -S "${LEXXOS_TEST_OUTPUT_DIR}/project"
                    -B "${LEXXOS_TEST_OUTPUT_DIR}/${_package}"
                    "-DLEXXOS_IDF_PATH=${_idf}"
                    "-DCMAKE_PREFIX_PATH=${LEXXOS_TEST_ROOT}/cmake"
                    "-DTEST_PACKAGE=${_package}" "-DTEST_TOOLS=${_tools}"
                    "-DCMAKE_AR=/usr/bin/ar" "-DCMAKE_RANLIB=/usr/bin/ranlib"
                    "-DCMAKE_OBJDUMP=/usr/bin/objdump" "-DCMAKE_LINKER=/usr/bin/ld"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _stdout
            ERROR_VARIABLE _stderr)
    if (NOT _result EQUAL 0)
        message(FATAL_ERROR "${_package} configuration failed:\n${_stdout}\n${_stderr}")
    endif ()
endforeach ()
