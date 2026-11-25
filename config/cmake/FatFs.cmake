include_guard(GLOBAL)

set(LEXXOS_FATFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/fatfs" CACHE PATH "Path to the FatFs package root")
set(LEXXOS_FATFS_SOURCE_DIR "${LEXXOS_FATFS_ROOT}/source" CACHE PATH "Path to the FatFs source directory")

if (NOT EXISTS "${LEXXOS_FATFS_SOURCE_DIR}/ff.c" OR NOT EXISTS "${LEXXOS_FATFS_SOURCE_DIR}/ff.h" OR NOT EXISTS "${LEXXOS_FATFS_SOURCE_DIR}/diskio.h")
    message(FATAL_ERROR
            "FatFs is enabled, but the expected source files were not found.\n"
            "Expected: ${LEXXOS_FATFS_SOURCE_DIR}/ff.c\n"
            "Expected: ${LEXXOS_FATFS_SOURCE_DIR}/ff.h\n"
            "Expected: ${LEXXOS_FATFS_SOURCE_DIR}/diskio.h\n"
            "Install FatFs into third_party/fatfs, for example:\n"
            "  mkdir -p third_party/fatfs\n"
            "  curl -L -o /tmp/fatfs-r0.16.zip https://elm-chan.org/fsw/ff/arc/ff16.zip\n"
            "  unzip /tmp/fatfs-r0.16.zip -d third_party/fatfs\n"
            "Or set -DLEXXOS_FATFS_SOURCE_DIR=/path/to/fatfs/source."
    )
endif ()

set(LEXXOS_FATFS_SOURCES
        "${LEXXOS_FATFS_SOURCE_DIR}/ff.c"
)

if (EXISTS "${LEXXOS_FATFS_SOURCE_DIR}/ffunicode.c")
    list(APPEND LEXXOS_FATFS_SOURCES "${LEXXOS_FATFS_SOURCE_DIR}/ffunicode.c")
endif ()
if (EXISTS "${LEXXOS_FATFS_SOURCE_DIR}/ffsystem.c")
    list(APPEND LEXXOS_FATFS_SOURCES "${LEXXOS_FATFS_SOURCE_DIR}/ffsystem.c")
endif ()

# Fast seek changes FatFs code only when a FIL has an explicit cluster map.
# The RK3506/Lyra profile opts in; every other board retains ordinary seeking.
set(_lexxos_fatfs_fastseek_default OFF)
if (LEXXOS_PLATFORM STREQUAL "rk3506")
    set(_lexxos_fatfs_fastseek_default ON)
endif ()
option(LEXXOS_FATFS_FASTSEEK "Enable optional FatFs cluster link maps for large .lexx files"
        ${_lexxos_fatfs_fastseek_default})

add_library(fatfs STATIC ${LEXXOS_FATFS_SOURCES})
target_include_directories(fatfs PUBLIC "${LEXXOS_FATFS_SOURCE_DIR}")
target_compile_features(fatfs PRIVATE c_std_99)

# Include the external configuration overlay in all FatFs consumers.
# PUBLIC is required because FF_USE_FASTSEEK changes the size of FIL.
include("${CMAKE_CURRENT_LIST_DIR}/FatFsConfigOverride.cmake")
lexxos_apply_fatfs_config(fatfs PUBLIC)

if (LEXXOS_PLATFORM STREQUAL "stm32f4")
    target_compile_options(fatfs
            PRIVATE
            -mcpu=cortex-m4
            -mthumb
            -mfpu=fpv4-sp-d16
            -mfloat-abi=hard
    )
endif ()
