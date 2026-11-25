# Prepare the patched RP2350 SMP port outside the FreeRTOS submodule.

set(LEXXOS_RP2350_SMP_PORT_PATCH
    "${CMAKE_CURRENT_LIST_DIR}/patches/freertos-rp2350-smp-port.patch")
set(LEXXOS_RP2350_SMP_KERNEL_PATCH
    "${CMAKE_CURRENT_LIST_DIR}/patches/freertos-rp2350-smp-kernel.patch")

function(lexxos_prepare_rp2350_smp_freertos_port source_kernel_directory source_port_directory output_kernel_directory output_port_directory)
    if (NOT EXISTS "${source_port_directory}/CMakeLists.txt")
        message(FATAL_ERROR "FreeRTOS RP2350 SMP port was not found: ${source_port_directory}")
    endif ()
    if (NOT EXISTS "${LEXXOS_RP2350_SMP_PORT_PATCH}" OR NOT EXISTS "${LEXXOS_RP2350_SMP_KERNEL_PATCH}")
        message(FATAL_ERROR "FreeRTOS RP2350 SMP patches were not found")
    endif ()

    file(REMOVE_RECURSE "${output_kernel_directory}")
    file(MAKE_DIRECTORY "${output_kernel_directory}")
    file(COPY "${source_kernel_directory}/" DESTINATION "${output_kernel_directory}")

    file(REMOVE_RECURSE "${output_port_directory}")
    file(MAKE_DIRECTORY "${output_port_directory}")
    file(COPY "${source_port_directory}/" DESTINATION "${output_port_directory}")

    execute_process(
        COMMAND patch --batch --forward --ignore-whitespace -p1 --input "${LEXXOS_RP2350_SMP_PORT_PATCH}"
        WORKING_DIRECTORY "${output_port_directory}"
        RESULT_VARIABLE patch_result
        OUTPUT_VARIABLE patch_output
        ERROR_VARIABLE patch_error
    )
    if (NOT patch_result EQUAL 0)
        message(FATAL_ERROR
            "Could not apply FreeRTOS RP2350 SMP port patch to ${output_port_directory}:\n"
            "${patch_output}${patch_error}")
    endif ()

    execute_process(
        COMMAND patch --batch --forward --ignore-whitespace -p1 --input "${LEXXOS_RP2350_SMP_KERNEL_PATCH}"
        WORKING_DIRECTORY "${output_kernel_directory}"
        RESULT_VARIABLE kernel_patch_result
        OUTPUT_VARIABLE kernel_patch_output
        ERROR_VARIABLE kernel_patch_error
    )
    if (NOT kernel_patch_result EQUAL 0)
        message(FATAL_ERROR
            "Could not apply FreeRTOS RP2350 SMP kernel patch to ${output_kernel_directory}:\n"
            "${kernel_patch_output}${kernel_patch_error}")
    endif ()
endfunction()
