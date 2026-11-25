include_guard(GLOBAL)

# Find this package before project() in an ESP-IDF application.
if (PROJECT_NAME)
    message(FATAL_ERROR "Find LexxIDF before project() in an ESP-IDF project")
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/LexxIDFEnvironment.cmake")
lexxos_import_espidf_export_environment("$ENV{IDF_PATH}/export.sh")
lexxos_configure_espidf_python()

# ESP-IDF resolves IDF_TARGET from the cache, environment and sdkconfig files
# when project.cmake is included. Resolve the compiler for that target before
# the application calls project().
if (NOT COMMAND idf_build_process)
    include("$ENV{IDF_PATH}/tools/cmake/project.cmake")
endif ()

lexxos_configure_espidf_toolchain()
if (COMMAND idf_build_set_property AND PYTHON)
    idf_build_set_property(PYTHON "${PYTHON}")
endif ()
lexxos_configure_espidf_subproject_toolchain()
