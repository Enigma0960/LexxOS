# Производитель SDK: только целевой чип, без платы и примера приложения.
get_filename_component(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LEXXOS_EDITION "ce" CACHE STRING "LexxOS edition" FORCE)
set(LEXXOS_PLATFORM "espidf" CACHE STRING "CE platform")
set(IDF_TARGET "esp32" CACHE STRING "CE SDK target")
if (NOT LEXXOS_PLATFORM STREQUAL "espidf" OR NOT IDF_TARGET STREQUAL "esp32")
    message(FATAL_ERROR "This SDK build supports espidf / esp32 only")
endif ()
include("${LEXXOS_ROOT_DIR}/config/cmake/Version.cmake")
include("${LEXXOS_ROOT_DIR}/config/cmake/platforms/espidf_prepare.cmake")

function(lexxos_add_ce_sdk_target)
    set(LEXXOS_CE_SDK_OUTPUT "${CMAKE_BINARY_DIR}/sdk/esp32" CACHE PATH "Exported CE SDK directory")
    add_custom_target(ce-sdk
            COMMAND "${PYTHON}" "${LEXXOS_ROOT_DIR}/tools/export_ce_sdk.py"
                    --root "${LEXXOS_ROOT_DIR}" --build "${CMAKE_BINARY_DIR}"
                    --archive "$<TARGET_FILE:__idf_lexxos_ce>"
                    --cooperative-archive "$<TARGET_FILE:__idf_lexxos_ce_cooperative>"
                    --espidf-archive "$<TARGET_FILE:__idf_lexxos_ce_espidf>"
                    --freertos-archive "$<TARGET_FILE:__idf_lexxos_ce_freertos>"
                    --compiler "${CMAKE_CXX_COMPILER}" --idf-path "$ENV{IDF_PATH}"
                    --output "${LEXXOS_CE_SDK_OUTPUT}"
            DEPENDS __idf_lexxos_ce __idf_lexxos_ce_cooperative __idf_lexxos_ce_espidf __idf_lexxos_ce_freertos VERBATIM)
endfunction()
