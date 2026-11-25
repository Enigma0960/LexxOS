include_guard(GLOBAL)

# Compile lwjson directly into the consuming target. This works for both
# native CMake targets and ESP-IDF components (which register their own target).
get_filename_component(LEXXOS_LWJSON_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../../third_party/lwjson" ABSOLUTE)

function(lexxos_attach_lwjson target_name)
    if (NOT LEXXOS_FEATURE_LWJSON)
        return()
    endif ()

    set(_lwjson_src "${LEXXOS_LWJSON_ROOT}/lwjson/src")
    set(_lwjson_opts "${_lwjson_src}/include/lwjson/lwjson_opts_template.h")
    if (NOT EXISTS "${_lwjson_src}/lwjson/lwjson.c" OR NOT EXISTS "${_lwjson_opts}")
        message(FATAL_ERROR
                "LEXXOS_FEATURE_LWJSON=ON but the lwjson submodule is missing. "
                "Run: git submodule update --init --recursive")
    endif ()

    set(_lwjson_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/lwjson")
    file(MAKE_DIRECTORY "${_lwjson_generated}")
    configure_file("${_lwjson_opts}" "${_lwjson_generated}/lwjson_opts.h" COPYONLY)

    target_sources(${target_name} PRIVATE
            "${_lwjson_src}/lwjson/lwjson.c"
            "${_lwjson_src}/lwjson/lwjson_stream.c"
            "${_lwjson_src}/lwjson/lwjson_serializer.c"
            "${_lwjson_src}/lwjson/lwjson_utils.c"
    )
    target_include_directories(${target_name} PRIVATE
            "${_lwjson_src}/include"
            "${_lwjson_generated}"
    )
endfunction()
