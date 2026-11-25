include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)

# Component Manager fixes this name relative to PROJECT_DIR. Keep its data
# under work and expose only the compatibility link required by ESP-IDF.
set(_lexxos_managed_data "${LEXXOS_ROOT_DIR}/work/espidf/managed_components")
set(_lexxos_managed_link "${LEXXOS_ROOT_DIR}/managed_components")
file(MAKE_DIRECTORY "${_lexxos_managed_data}")
if (EXISTS "${_lexxos_managed_link}" AND NOT IS_SYMLINK "${_lexxos_managed_link}")
    message(FATAL_ERROR "Move existing managed_components into work/espidf/managed_components before configuring ESP-IDF; see docs/project-layout.md")
elseif (NOT IS_SYMLINK "${_lexxos_managed_link}")
    file(CREATE_LINK "${_lexxos_managed_data}" "${_lexxos_managed_link}" SYMBOLIC RESULT _lexxos_link_result)
    if (NOT _lexxos_link_result STREQUAL "0")
        message(FATAL_ERROR "Cannot create ESP-IDF managed_components link: ${_lexxos_link_result}")
    endif ()
endif ()


include("${LEXXOS_ROOT_DIR}/cmake/LexxIDFEnvironment.cmake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (LEXXOS_EDITION STREQUAL "ce")
    set(LEXXOS_ESPIDF_COMPONENT_DIR "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos_ce")
else ()
    set(LEXXOS_ESPIDF_COMPONENT_DIR "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos")
endif ()

if (NOT EXISTS "${LEXXOS_ESPIDF_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "ESP-IDF LexxOS component is missing: ${LEXXOS_ESPIDF_COMPONENT_DIR}/CMakeLists.txt")
endif ()

set(EXTRA_COMPONENT_DIRS
        "${LEXXOS_ESPIDF_COMPONENT_DIR}"
        CACHE STRING "Additional ESP-IDF components used by LexxOS" FORCE
)

if (LEXXOS_EDITION STREQUAL "ce")
    list(APPEND EXTRA_COMPONENT_DIRS
            "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos_ce_espidf"
            "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos_ce_freertos"
            "${LEXXOS_ROOT_DIR}/ports/espidf/components/lexxos_ce_cooperative"
            "${LEXXOS_ROOT_DIR}/ports/espidf/ce_sdk/main")
    set(COMPONENTS main)
endif ()

# ESP-IDF performs an early component-requirements configure pass before the
# normal LexxOS project configure state is fully visible.  Keep the selected
# board in a dedicated cache/environment hint so the component CMakeLists does
# not fall back from m5stack_cardputer_adv to the generic ESP32-S3 Cardputer
# profile during that early pass.
if (NOT LEXXOS_EDITION STREQUAL "ce")
    set(LEXXOS_ESPIDF_BOARD "${LEXXOS_BOARD}" CACHE STRING "LexxOS board profile selected for ESP-IDF component discovery" FORCE)
    set(ENV{LEXXOS_ESPIDF_BOARD} "${LEXXOS_BOARD}")
    set(ENV{LEXXOS_BOARD} "${LEXXOS_BOARD}")
endif ()

if (LEXXOS_EDITION STREQUAL "ce")
    set(IDF_TARGET "esp32" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.ce.defaults")
elseif (LEXXOS_BOARD STREQUAL "m5stack_cardputer" OR LEXXOS_BOARD STREQUAL "m5stack_cardputer_adv")
    set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.cardputer.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_m5stack_cardputer_8mb.csv")
elseif (LEXXOS_BOARD STREQUAL "jc_esp32p4_m3_dev")
    set(IDF_TARGET "esp32p4" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.jc_esp32p4_m3_dev.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_jc_esp32p4_m3_dev_16mb.csv")
else ()
    set(IDF_TARGET "esp32" CACHE STRING "ESP-IDF target" FORCE)
    set(LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS "${LEXXOS_ROOT_DIR}/ports/espidf/sdkconfig.defaults")
    set(LEXXOS_ESPIDF_PARTITION_TABLE "${LEXXOS_ROOT_DIR}/ports/espidf/partitions_m5stack_basic_v1_4mb.csv")
endif ()

if (NOT EXISTS "${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}")
    message(FATAL_ERROR "ESP-IDF sdkconfig defaults are missing: ${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}")
endif ()

set(SDKCONFIG_DEFAULTS "${LEXXOS_ESPIDF_SDKCONFIG_DEFAULTS}"
        CACHE STRING "ESP-IDF sdkconfig defaults for LexxOS" FORCE)
if (NOT LEXXOS_EDITION STREQUAL "ce")
    if (NOT EXISTS "${LEXXOS_ESPIDF_PARTITION_TABLE}")
        message(FATAL_ERROR "ESP-IDF partition table is missing: ${LEXXOS_ESPIDF_PARTITION_TABLE}")
    endif ()
    set(PARTITION_TABLE_CUSTOM_FILENAME "${LEXXOS_ESPIDF_PARTITION_TABLE}"
            CACHE STRING "ESP-IDF custom partition table for LexxOS" FORCE)
endif ()

# The root CMake entry point must not create or reuse sdkconfig in the repository
# root. Keep ESP-IDF configuration inside the selected build directory, so the
# repository can host multiple platform profiles without source-tree side effects.
set(SDKCONFIG
        "${CMAKE_BINARY_DIR}/sdkconfig"
        CACHE FILEPATH "ESP-IDF sdkconfig generated for this LexxOS build directory" FORCE
)

set(PROJECT_VER "${LEXXOS_VERSION}")

lexxos_import_espidf_export_environment("$ENV{IDF_PATH}/export.sh")
lexxos_configure_espidf_python()
lexxos_configure_espidf_toolchain()

# Keep independent dependency resolutions for Xtensa and ESP32-P4 targets.
# A single root lock is otherwise rewritten on every board switch.
file(MAKE_DIRECTORY "${LEXXOS_ROOT_DIR}/ports/espidf/locks")
if (LEXXOS_EDITION STREQUAL "ce")
    set(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
else ()
    set(DEPENDENCIES_LOCK "${LEXXOS_ROOT_DIR}/ports/espidf/locks/${IDF_TARGET}.lock")
endif ()
include("$ENV{IDF_PATH}/tools/cmake/project.cmake")
idf_build_set_property(DEPENDENCIES_LOCK "${DEPENDENCIES_LOCK}")
if (COMMAND idf_build_set_property AND NOT PYTHON STREQUAL "")
    idf_build_set_property(PYTHON "${PYTHON}")
endif ()

lexxos_configure_espidf_subproject_toolchain()
