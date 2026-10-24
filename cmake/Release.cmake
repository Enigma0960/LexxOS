include_guard(GLOBAL)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(LEXXOS_RELEASE_OUTPUT "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Release archive destination")
option(LEXXOS_RELEASE_ALLOW_DIRTY "Build development archives from uncommitted working files" OFF)
set(_lexxos_release_args --output "${LEXXOS_RELEASE_OUTPUT}")
if (LEXXOS_RELEASE_ALLOW_DIRTY)
    list(APPEND _lexxos_release_args --allow-dirty)
endif ()
add_custom_target(release-archives
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/release.py" ${_lexxos_release_args}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
        COMMENT "Build clean, isolated firmware and apps release archives")
