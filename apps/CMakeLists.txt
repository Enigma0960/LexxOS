find_package(Python3 REQUIRED COMPONENTS Interpreter)

# These tools target the guest VM, independently of the firmware toolchain.
find_program(LEXXOS_APP_CXX NAMES riscv64-unknown-elf-g++ riscv64-elf-g++
        riscv32-unknown-elf-g++ riscv-none-elf-g++ clang++ REQUIRED)
get_filename_component(_app_tool_dir "${LEXXOS_APP_CXX}" DIRECTORY)
get_filename_component(_app_compiler "${LEXXOS_APP_CXX}" NAME)
set(_app_target_flags)
if (_app_compiler MATCHES "clang")
    set(_app_target_flags --target=riscv32 -fuse-ld=lld)
    find_program(LEXXOS_APP_OBJCOPY NAMES llvm-objcopy HINTS "${_app_tool_dir}" REQUIRED)
else ()
    string(REGEX REPLACE "g\\+\\+$" "objcopy" _app_objcopy "${_app_compiler}")
    find_program(LEXXOS_APP_OBJCOPY NAMES "${_app_objcopy}" HINTS "${_app_tool_dir}" REQUIRED)
endif ()

set(LEXXOS_APPS_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/build/apps")
set(_app_linker "${PROJECT_SOURCE_DIR}/examples/lexx/lexx.ld")
set(_app_packer "${PROJECT_SOURCE_DIR}/tools/lexx_pack.py")
add_custom_target(apps ALL)

function(lexxos_add_app name)
    cmake_parse_arguments(APP "" "MEMORY;STACK;CAPABILITIES;MANIFEST" "SOURCES" ${ARGN})
    set(work_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    set(objects)
    foreach(source IN LISTS APP_SOURCES)
        set(object "${work_dir}/${source}.o")
        get_filename_component(object_dir "${object}" DIRECTORY)
        add_custom_command(OUTPUT "${object}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${object_dir}"
                COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags}
                    -std=c++20 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin
                    -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
                    -ffunction-sections -fdata-sections -Os -Wall -Wextra
                    "-I${PROJECT_SOURCE_DIR}/sdk" -MMD -MF "${object}.d"
                    -c "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${object}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
                DEPFILE "${object}.d"
                VERBATIM)
        list(APPEND objects "${object}")
    endforeach ()
    add_custom_command(OUTPUT "${work_dir}/${name}.elf"
            COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags} -march=rv32i -mabi=ilp32
                -nostdlib -static "-Wl,-T,${_app_linker}" -Wl,--gc-sections
                "-Wl,-Map,${work_dir}/${name}.map" -o "${work_dir}/${name}.elf" ${objects}
            DEPENDS ${objects} "${_app_linker}"
            BYPRODUCTS "${work_dir}/${name}.map"
            VERBATIM)
    add_custom_command(OUTPUT "${work_dir}/${name}.bin"
            COMMAND "${LEXXOS_APP_OBJCOPY}" -O binary "${work_dir}/${name}.elf" "${work_dir}/${name}.bin"
            DEPENDS "${work_dir}/${name}.elf"
            VERBATIM)
    add_custom_command(OUTPUT "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEXXOS_APPS_OUTPUT_DIR}"
            COMMAND "${Python3_EXECUTABLE}" "${_app_packer}" "${work_dir}/${name}.bin"
                "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx" --memory "${APP_MEMORY}"
                --stack "${APP_STACK}" --cap "${APP_CAPABILITIES}"
                --manifest "${CMAKE_CURRENT_SOURCE_DIR}/${APP_MANIFEST}"
            DEPENDS "${work_dir}/${name}.bin" "${_app_packer}" "${CMAKE_CURRENT_SOURCE_DIR}/${APP_MANIFEST}"
            VERBATIM)
    add_custom_target(app-${name} DEPENDS "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx")
    add_dependencies(apps app-${name})
endfunction()

add_subdirectory(fileman)
add_subdirectory(pacman)
add_subdirectory(snake)
add_subdirectory(tetris)
