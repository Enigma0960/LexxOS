if (NOT LEXXOS_PLATFORM STREQUAL "host")
    return()
endif ()

add_subdirectory(cpp)

# Exercise build-info generation independently of a full ESP-IDF toolchain.
add_test(NAME lexxos_build_info_native_version
        COMMAND ${CMAKE_COMMAND}
        "-DLEXXOS_TEST_ROOT=${PROJECT_SOURCE_DIR}"
        "-DLEXXOS_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/build-info-native"
        -DLEXXOS_TEST_PLATFORM=host
        -DLEXXOS_TEST_EXPECTED_VERSION=${LEXXOS_VERSION}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_info_version_test.cmake")
add_test(NAME lexxos_build_info_espidf_version
        COMMAND ${CMAKE_COMMAND}
        "-DLEXXOS_TEST_ROOT=${PROJECT_SOURCE_DIR}"
        "-DLEXXOS_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/build-info-espidf"
        -DLEXXOS_TEST_PLATFORM=espidf
        -DLEXXOS_TEST_PROJECT_VER=9.8.7
        -DLEXXOS_TEST_EXPECTED_VERSION=9.8.7
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_info_version_test.cmake")


if (LEXXOS_ENABLE_STATIC_CHECKS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_test(NAME lexxos_cpp_layout_check
            COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/check_cpp_layout.py
            ${PROJECT_SOURCE_DIR}
    )

    add_test(NAME lexxos_markdown_check
            COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/check_markdown.py
            ${PROJECT_SOURCE_DIR}
    )

    add_test(NAME lexxos_board_preset_check
            COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/check_board_presets.py
            ${PROJECT_SOURCE_DIR}
    )
endif ()

# This smoke uses help/exit; builds without SYSTEM intentionally omit both.
if (UNIX AND LEXXOS_COMMANDS_SYSTEM)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME lexxos_host_stdin
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/host_stdin_test.py $<TARGET_FILE:lexxos>
            ${LEXXOS_GENERATED_INCLUDE_DIR}/commands/commands.json)
    set_tests_properties(lexxos_host_stdin PROPERTIES TIMEOUT 15)
endif ()

# Packer metadata tests need only the host Python interpreter (no RISC-V toolchain).
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    add_test(NAME lexxos_command_catalog_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/tests/python -p test_command_catalog.py)
    add_test(NAME lexxos_command_docs_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/tests/python -p test_command_docs.py)
    add_test(
            NAME lexxos_lexx_manifest_pack_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/tests/python -p test_lexx_pack.py
    )
    add_test(
            NAME lexxos_release_packaging_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/tests/python -p test_release.py
    )

endif ()
