if (NOT DEFINED LEXXOS_TEST_ROOT OR NOT DEFINED LEXXOS_TEST_OUTPUT_DIR
        OR NOT DEFINED LEXXOS_TEST_PLATFORM OR NOT DEFINED LEXXOS_TEST_EXPECTED_VERSION)
    message(FATAL_ERROR "Missing build-info test parameters")
endif ()

set(LEXXOS_PLATFORM "${LEXXOS_TEST_PLATFORM}")
set(LEXXOS_GENERATED_INCLUDE_DIR "${LEXXOS_TEST_OUTPUT_DIR}")
if (DEFINED LEXXOS_TEST_PROJECT_VER)
    set(PROJECT_VER "${LEXXOS_TEST_PROJECT_VER}")
endif ()

include("${LEXXOS_TEST_ROOT}/cmake/BuildInfo.cmake")
file(READ "${LEXXOS_BUILD_INFO_FILE}" generated)

set(expected "inline constexpr char version[] = \"${LEXXOS_TEST_EXPECTED_VERSION}\";")
string(FIND "${generated}" "${expected}" matchIndex)
if (matchIndex EQUAL -1)
    message(FATAL_ERROR "Generated build_info.h does not contain ${expected}")
endif ()

message(STATUS "build_info.h: ${LEXXOS_TEST_PLATFORM} version ${LEXXOS_TEST_EXPECTED_VERSION}")
