function(lexxos_validate_target)
    if (NOT DEFINED LEXXOS_BOARD_PLATFORM OR LEXXOS_BOARD_PLATFORM STREQUAL "")
        message(FATAL_ERROR "cmake/Boards.cmake must be included before lexxos_validate_target()")
    endif ()

    if (NOT LEXXOS_PLATFORM STREQUAL LEXXOS_BOARD_PLATFORM)
        message(FATAL_ERROR
                "LEXXOS_BOARD=${LEXXOS_BOARD} requires LEXXOS_PLATFORM=${LEXXOS_BOARD_PLATFORM}, "
                "but current LEXXOS_PLATFORM=${LEXXOS_PLATFORM}")
    endif ()

    if (NOT LEXXOS_PLATFORM MATCHES "^(host|pico|sim|stm32f4|espidf)$")
        message(FATAL_ERROR "Unsupported LEXXOS_PLATFORM: ${LEXXOS_PLATFORM}")
    endif ()
endfunction()
