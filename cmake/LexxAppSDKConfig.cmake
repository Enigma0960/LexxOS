if (TARGET LexxAppSDK::Headers)
    return()
endif ()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(_lexx_app_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(LexxAppSDK::Headers INTERFACE IMPORTED GLOBAL)
target_include_directories(LexxAppSDK::Headers INTERFACE "${_lexx_app_root}/apps/sdk")
target_compile_features(LexxAppSDK::Headers INTERFACE cxx_std_20)

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

if (NOT DEFINED LEXXOS_APPS_OUTPUT_DIR)
    set(LEXXOS_APPS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/apps")
endif ()
if (NOT DEFINED _lexx_app_aggregate)
    set(_lexx_app_aggregate lexx-apps)
endif ()
# The SDK guest runtime is compiled once and linked into each standalone .lexx.
# Keep it separate from board/host runtime: the VM has its own RV32I ABI.
set(_sdk_runtime_source "${_lexx_app_root}/apps/sdk/lexx/runtime.cpp")
set(_sdk_runtime_object "${CMAKE_CURRENT_BINARY_DIR}/lexx-sdk-runtime.o")
add_custom_command(OUTPUT "${_sdk_runtime_object}"
        COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags}
        -std=c++20 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin
        -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
        -ffunction-sections -fdata-sections -Os -Wall -Wextra
        "-I${_lexx_app_root}/apps/sdk" -MMD -MF "${_sdk_runtime_object}.d"
        -c "${_sdk_runtime_source}" -o "${_sdk_runtime_object}"
        DEPENDS "${_sdk_runtime_source}"
        DEPFILE "${_sdk_runtime_object}.d"
        VERBATIM)
add_custom_target(lexx-sdk-runtime DEPENDS "${_sdk_runtime_object}")

add_custom_target(${_lexx_app_aggregate} ALL)
set_target_properties(lexx-sdk-runtime PROPERTIES
        LEXX_APP_COMPILER "${LEXXOS_APP_CXX}"
        LEXX_APP_TARGET_FLAGS "${_app_target_flags}"
        LEXX_APP_OBJCOPY "${LEXXOS_APP_OBJCOPY}"
        LEXX_APP_PYTHON "${Python3_EXECUTABLE}"
        LEXX_APP_RUNTIME_OBJECT "${_sdk_runtime_object}"
        LEXX_APP_OUTPUT_DIR "${LEXXOS_APPS_OUTPUT_DIR}"
        LEXX_APP_AGGREGATE "${_lexx_app_aggregate}")

function(lexxos_add_app name)
    cmake_parse_arguments(APP "" "MEMORY;STACK;CAPABILITIES;MANIFEST;RESOURCE_DIRECTORY"
            "SOURCES;C_SOURCES;INCLUDE_DIRECTORIES;RESOURCE_EXTRA_DEPS" ${ARGN})
    if (APP_UNPARSED_ARGUMENTS OR NOT APP_MANIFEST OR NOT (APP_SOURCES OR APP_C_SOURCES))
        message(FATAL_ERROR "lexxos_add_app(${name}) requires MANIFEST and SOURCES or C_SOURCES")
    endif ()
    get_filename_component(_lexx_app_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    get_target_property(LEXXOS_APP_CXX lexx-sdk-runtime LEXX_APP_COMPILER)
    get_target_property(_app_target_flags lexx-sdk-runtime LEXX_APP_TARGET_FLAGS)
    get_target_property(LEXXOS_APP_OBJCOPY lexx-sdk-runtime LEXX_APP_OBJCOPY)
    get_target_property(_python lexx-sdk-runtime LEXX_APP_PYTHON)
    get_target_property(_sdk_runtime_object lexx-sdk-runtime LEXX_APP_RUNTIME_OBJECT)
    get_target_property(LEXXOS_APPS_OUTPUT_DIR lexx-sdk-runtime LEXX_APP_OUTPUT_DIR)
    get_target_property(_lexx_app_aggregate lexx-sdk-runtime LEXX_APP_AGGREGATE)
    set(_app_linker "${_lexx_app_root}/apps/sdk/lexx/lexx.ld")
    set(_app_packer "${_lexx_app_root}/tools/lexx_pack.py")
    set(_include_flags)
    foreach (_include IN LISTS APP_INCLUDE_DIRECTORIES)
        get_filename_component(_include "${_include}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND _include_flags "-I${_include}")
    endforeach ()
    set(work_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}")

    # A changed source file must repack .lexx without rebuilding the RV32I image.
    get_filename_component(_manifest_file "${APP_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_manifest_dir "${_manifest_file}" DIRECTORY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest_file}")
    if (NOT APP_RESOURCE_DIRECTORY OR NOT DEFINED APP_MEMORY
            OR NOT DEFINED APP_STACK OR NOT DEFINED APP_CAPABILITIES)
        file(READ "${_manifest_file}" _manifest_json)
    endif ()
    set(_pack_options)
    if (NOT DEFINED APP_MEMORY)
        string(JSON APP_MEMORY ERROR_VARIABLE _memory_error GET "${_manifest_json}" memory guest_bytes)
    endif ()
    if (NOT DEFINED APP_STACK)
        string(JSON APP_STACK ERROR_VARIABLE _stack_error GET "${_manifest_json}" memory stack_bytes)
    endif ()
    if (NOT APP_MEMORY MATCHES "-NOTFOUND$")
        list(APPEND _pack_options --memory "${APP_MEMORY}")
    endif ()
    if (NOT APP_STACK MATCHES "-NOTFOUND$")
        list(APPEND _pack_options --stack "${APP_STACK}")
    endif ()
    if (DEFINED APP_CAPABILITIES)
        list(APPEND _pack_options "--cap=${APP_CAPABILITIES}")
    else ()
        string(JSON _cap_count ERROR_VARIABLE _cap_error LENGTH "${_manifest_json}" capabilities)
        if (_cap_error STREQUAL "NOTFOUND" AND _cap_count GREATER 0)
            math(EXPR _cap_last "${_cap_count} - 1")
            foreach (_cap_index RANGE 0 ${_cap_last})
                string(JSON _cap GET "${_manifest_json}" capabilities ${_cap_index})
                list(APPEND _pack_options "--cap=${_cap}")
            endforeach ()
        endif ()
    endif ()
    set(_resource_deps)
    if (APP_RESOURCE_DIRECTORY)
        # Large sprite catalogs are tracked as a directory, not expanded by
        # thousands of O(N^2) string(JSON) operations at configure time.
        get_filename_component(_resource_directory "${APP_RESOURCE_DIRECTORY}" ABSOLUTE
                BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(GLOB_RECURSE _resource_deps CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE "${_resource_directory}/*")
    else ()
        string(JSON _resources_type ERROR_VARIABLE _resources_error TYPE "${_manifest_json}" resources)
        if (_resources_error STREQUAL "NOTFOUND")
            if (NOT _resources_type STREQUAL "OBJECT")
                message(FATAL_ERROR "${_manifest_file}: resources must be a JSON object")
            endif ()
            string(JSON _resources_count LENGTH "${_manifest_json}" resources)
            if (_resources_count GREATER 0)
                math(EXPR _resources_last "${_resources_count} - 1")
                foreach (_resource_index RANGE 0 ${_resources_last})
                    string(JSON _resource_alias MEMBER "${_manifest_json}" resources ${_resource_index})
                    string(JSON _resource_source GET "${_manifest_json}" resources "${_resource_alias}")
                    get_filename_component(_resource_path "${_resource_source}" ABSOLUTE BASE_DIR "${_manifest_dir}")
                    list(APPEND _resource_deps "${_resource_path}")
                endforeach ()
            endif ()
        endif ()
    endif ()

    foreach (_dependency IN LISTS APP_RESOURCE_EXTRA_DEPS)
        get_filename_component(_dependency "${_dependency}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND _resource_deps "${_dependency}")
    endforeach ()

    set(objects)
    foreach (source IN LISTS APP_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        get_filename_component(base "${source}" NAME)
        string(SHA256 source_id "${source}")
        set(object "${work_dir}/${source_id}/${base}.o")
        get_filename_component(object_dir "${object}" DIRECTORY)
        add_custom_command(OUTPUT "${object}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${object_dir}"
                COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags}
                -std=c++20 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin
                -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
                -ffunction-sections -fdata-sections -Os -Wall -Wextra
                "-I${_lexx_app_root}/apps/sdk"
                "-I${_lexx_app_root}/apps/sdk/lexx/compat"
                "-I${_lexx_app_root}/third_party/lwjson/lwjson/src/include"
                -DLWJSON_IGNORE_USER_OPTS -DLWJSON_CFG_INT_TYPE=int32_t
                -DLWJSON_CFG_REAL_TYPE=int32_t -MMD -MF "${object}.d"
                ${_include_flags} -c "${source}" -o "${object}"
                DEPENDS "${source}"
                DEPFILE "${object}.d"
                VERBATIM)
        list(APPEND objects "${object}")
    endforeach ()
    foreach (source IN LISTS APP_C_SOURCES)
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        get_filename_component(base "${source}" NAME)
        string(SHA256 source_id "${source}")
        set(object "${work_dir}/guest-c/${source_id}/${base}.o")
        add_custom_command(OUTPUT "${object}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${work_dir}/guest-c/${source_id}"
                COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags}
                -x c -std=c11 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin
                -ffunction-sections -fdata-sections -Os -Wall -Wextra
                -DLWJSON_IGNORE_USER_OPTS -DLWJSON_CFG_INT_TYPE=int32_t
                -DLWJSON_CFG_REAL_TYPE=int32_t
                "-I${_lexx_app_root}/third_party/lwjson/lwjson/src/include"
                "-I${_lexx_app_root}/apps/sdk/lexx/compat"
                "-I${_lexx_app_root}/apps/sdk" ${_include_flags}
                -MMD -MF "${object}.d" -c "${source}" -o "${object}"
                DEPENDS "${source}" DEPFILE "${object}.d" VERBATIM)
        list(APPEND objects "${object}")
    endforeach ()
    add_custom_command(OUTPUT "${work_dir}/${name}.elf"
            COMMAND "${LEXXOS_APP_CXX}" ${_app_target_flags} -march=rv32i -mabi=ilp32
            -nostdlib -static "-Wl,-T,${_app_linker}" -Wl,--gc-sections
            "-Wl,-Map,${work_dir}/${name}.map" -o "${work_dir}/${name}.elf" ${objects} "${_sdk_runtime_object}"
            DEPENDS ${objects} "${_sdk_runtime_object}" "${_app_linker}"
            BYPRODUCTS "${work_dir}/${name}.map"
            VERBATIM)
    add_custom_command(OUTPUT "${work_dir}/${name}.bin"
            COMMAND "${LEXXOS_APP_OBJCOPY}" -O binary "${work_dir}/${name}.elf" "${work_dir}/${name}.bin"
            DEPENDS "${work_dir}/${name}.elf"
            VERBATIM)
    add_custom_command(OUTPUT "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEXXOS_APPS_OUTPUT_DIR}"
            COMMAND "${_python}" "${_app_packer}" "${work_dir}/${name}.bin"
            "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx" ${_pack_options}
            --elf "${work_dir}/${name}.elf"
            --manifest "${_manifest_file}"
            DEPENDS "${work_dir}/${name}.bin" "${work_dir}/${name}.elf" "${_app_packer}" "${_manifest_file}" ${_resource_deps}
            VERBATIM)
    add_custom_target(app-${name} DEPENDS "${LEXXOS_APPS_OUTPUT_DIR}/${name}.lexx")
    add_dependencies(app-${name} lexx-sdk-runtime)
    add_dependencies(${_lexx_app_aggregate} app-${name})
endfunction()
