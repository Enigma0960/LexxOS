# RK3506: штатные board/feature-флаги, отдельные SDMMC/SPI1 и LCD SPI0.
# RK3506 — платформа; PicoCalc — общий профиль периферии.

# Cached board selectors имеют приоритет над LEXXOS_BOARD.
# При повторном использовании host cache явно выбираем Lyra.
set(LEXXOS_BOARD "picocalc_lyra" CACHE STRING "Target board profile" FORCE)
set(LEXXOS_BOARD_HOST_NATIVE OFF CACHE BOOL "Build for the native host board profile" FORCE)
set(LEXXOS_BOARD_PICOCALC_LYRA ON CACHE BOOL "Build for ClockworkPi PicoCalc with Luckfox Lyra" FORCE)
set(LEXXOS_EFFECTIVE_SD_BACKEND "spi")
set(LEXXOS_BUILD_FLAVOR "minimal")

# Общий реестр задаёт board ID, платформу, семейство и матрицу возможностей.
include("${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/Boards.cmake")

if (NOT LEXXOS_BOARD STREQUAL "picocalc_lyra" OR NOT LEXXOS_PLATFORM STREQUAL "rk3506")
    message(FATAL_ERROR "RK3506 bootstrap requires LEXXOS_BOARD=picocalc_lyra; clear the CMake cache or disable conflicting board options")
endif ()

# SdCardBackend подключает съёмную RW SD PicoCalc как sd1.
# Системная read-only SDMMC остаётся sd0 с явным GPT scan.
set(LEXXOS_BOARD_AVAILABLE_FATFS ON)
set(LEXXOS_BOARD_DEFAULT_FATFS ON)
set(LEXXOS_FEATURE_FATFS ON CACHE BOOL "RK3506 FatFs for removable SPI1 SD and read-only SDMMC" FORCE)
set(LEXXOS_FEATURE_HEALTH OFF CACHE BOOL "Disable unimplemented RK3506 status LED task" FORCE)
set(LEXXOS_FEATURE_GAMEBOY OFF CACHE BOOL "Game Boy is not supported yet on RK3506" FORCE)
# Explicitly limit the initial built-ins to hardware-independent commands.
set(LEXXOS_COMMANDS_SYSTEM ON CACHE BOOL "System shell programs" FORCE)
set(LEXXOS_COMMANDS_UTILITIES ON CACHE BOOL "General-purpose shell programs" FORCE)
set(LEXXOS_COMMANDS_DIAGNOSTICS ON CACHE BOOL "Hardware-independent diagnostics" FORCE)
set(LEXXOS_COMMANDS_ADMIN OFF CACHE BOOL "RK3506 admin commands (later)" FORCE)
set(LEXXOS_COMMANDS_APPLICATIONS OFF CACHE BOOL "RK3506 applications (later)")
set(LEXXOS_COMMANDS_EXPERIMENTAL OFF CACHE BOOL "RK3506 experimental commands (later)" FORCE)
