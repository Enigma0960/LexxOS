if (TARGET LexxCE::LexxCE)
    return()
endif ()

get_filename_component(_lexx_ce_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if (NOT LEXXOS_PLATFORM)
    set(LEXXOS_PLATFORM "$ENV{LEXXOS_PLATFORM}")
endif ()
if (NOT LEXXOS_PLATFORM)
    if (COMMAND idf_build_process OR IDF_TARGET OR DEFINED ENV{IDF_TARGET}
            OR (NOT PROJECT_NAME AND (LEXXOS_IDF_PATH OR IDF_PATH
                    OR NOT "$ENV{LEXXOS_IDF_PATH}" STREQUAL "" OR NOT "$ENV{IDF_PATH}" STREQUAL "")))
        set(LEXXOS_PLATFORM espidf)
    else ()
        set(LEXXOS_PLATFORM host)
    endif ()
endif ()

set(LEXXOS_CE_EXECUTION "cooperative" CACHE STRING "LexxCE scheduler")
set_property(CACHE LEXXOS_CE_EXECUTION PROPERTY STRINGS cooperative freertos)
if (LEXXOS_PLATFORM STREQUAL "espidf")
    if (NOT LEXXOS_CE_EXECUTION MATCHES "^(cooperative|freertos)$")
        set(LexxCE_FOUND FALSE)
        set(LexxCE_NOT_FOUND_MESSAGE "Set LEXXOS_CE_EXECUTION to cooperative or freertos")
        return()
    endif ()
    if (PROJECT_NAME)
        set(LexxCE_FOUND FALSE)
        set(LexxCE_NOT_FOUND_MESSAGE "Find LexxCE before project() in an ESP-IDF project")
        return()
    endif ()
    include("${CMAKE_CURRENT_LIST_DIR}/LexxIDFConfig.cmake")

    # ESP-IDF expands component requirements in a separate CMake process.
    set(ENV{_LEXX_CE_EXECUTION} "${LEXXOS_CE_EXECUTION}")
    set(_lexx_ce_components "${_lexx_ce_root}/ports/espidf/components")
    list(APPEND EXTRA_COMPONENT_DIRS
            "${_lexx_ce_components}/lexxos_ce"
            "${_lexx_ce_components}/lexxos_ce_espidf"
            "${_lexx_ce_components}/lexxos_ce_${LEXXOS_CE_EXECUTION}"
            "${CMAKE_CURRENT_LIST_DIR}/idf/lexxce")
    add_library(LexxCE::LexxCE INTERFACE IMPORTED GLOBAL)
    target_link_libraries(LexxCE::LexxCE INTERFACE idf::lexxce)
elseif (LEXXOS_PLATFORM STREQUAL "host" AND LEXXOS_CE_EXECUTION STREQUAL "cooperative")
    if (NOT TARGET lexxos_ce_cooperative)
        add_subdirectory("${_lexx_ce_root}/ce" "${CMAKE_CURRENT_BINARY_DIR}/_lexxce" EXCLUDE_FROM_ALL)
    endif ()
    add_library(LexxCE::LexxCE ALIAS lexxos_ce_cooperative)
else ()
    set(LexxCE_FOUND FALSE)
    set(LexxCE_NOT_FOUND_MESSAGE "Supported LexxCE platforms are espidf (ESP32) and host (cooperative)")
endif ()
