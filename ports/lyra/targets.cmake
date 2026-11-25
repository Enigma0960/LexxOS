# A complete SD image; normal builds still never write devices.
set(LEXXOS_LYRA_SD_SIZE_MIB "128" CACHE STRING "Size of the complete Lyra SD image in MiB")
set(LEXXOS_LYRA_SD_SIZE_BYTES "" CACHE STRING "Exact destination card size; overrides MiB size")
set(LEXXOS_LYRA_ROOTFS_IMAGE "" CACHE FILEPATH "Optional unchanged vendor rootfs to retain on SD")
set(_lyra_rootfs_args)
if (NOT LEXXOS_LYRA_SD_SIZE_BYTES STREQUAL "")
    list(APPEND _lyra_rootfs_args --size-bytes "${LEXXOS_LYRA_SD_SIZE_BYTES}")
endif ()
if (NOT LEXXOS_LYRA_ROOTFS_IMAGE STREQUAL "")
    list(APPEND _lyra_rootfs_args --rootfs "${LEXXOS_LYRA_ROOTFS_IMAGE}")
endif ()
add_custom_target(sd-image-rk3506
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ports/lyra/build.py"
        --kernel "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
        --uboot "${_rk3506_uboot_dir}/uboot.img"
        --output "${CMAKE_CURRENT_BINARY_DIR}/lexxos-sd.img"
        --size-mib "${LEXXOS_LYRA_SD_SIZE_MIB}" ${_lyra_rootfs_args}
        DEPENDS lexxos uboot-rk3506
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/lexxos-sd.img"
        "${CMAKE_CURRENT_BINARY_DIR}/lexxos-sd.json"
        "${CMAKE_CURRENT_BINARY_DIR}/lexxos-sd.img.sha256"
        USES_TERMINAL VERBATIM
        COMMENT "Assembling complete Lyra SD image (no device writes)")

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_lyra_flash_args)
    if (NOT LEXXOS_RK3506_FLASH_DISK STREQUAL "")
        list(APPEND _lyra_flash_args --disk "${LEXXOS_RK3506_FLASH_DISK}")
    endif ()
    if (NOT LEXXOS_LYRA_ROOTFS_IMAGE STREQUAL "")
        list(APPEND _lyra_flash_args --rootfs "${LEXXOS_LYRA_ROOTFS_IMAGE}")
    endif ()
    add_custom_target(flash-rk3506
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ports/lyra/flash.py"
            --kernel "${CMAKE_CURRENT_BINARY_DIR}/lexxos.bin"
            --uboot "${_rk3506_uboot_dir}/uboot.img"
            --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
            --backup-dir "${CMAKE_SOURCE_DIR}/work/backups/rk3506"
            ${_lyra_flash_args}
            DEPENDS lexxos uboot-rk3506
            USES_TERMINAL VERBATIM
            COMMENT "INTERACTIVE FULL SD ERASE: build image for selected card, backup, write and verify")
    add_custom_target(list-rk3506-disks
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ports/lyra/flash.py" --list
            USES_TERMINAL VERBATIM COMMENT "List removable cards for full installation, including blank media")
endif ()
