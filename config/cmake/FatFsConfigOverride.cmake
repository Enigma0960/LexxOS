include_guard(GLOBAL)

# FatFs ff.h includes the vendor ffconf.h only when FFCONF_DEF is not defined.
# Force-include LexxOS's wrapper BEFORE ff.h in every C/C++ translation unit,
# so ff.c and every consumer of FIL see exactly the same struct layout.
# No changes to the upstream FatFs sources or headers are necessary.
function(lexxos_apply_fatfs_config target visibility)
    target_compile_definitions(${target} ${visibility}
            LEXXOS_FATFS_FASTSEEK=$<BOOL:${LEXXOS_FATFS_FASTSEEK}>)

    if (NOT LEXXOS_FATFS_FASTSEEK)
        return()
    endif ()

    get_filename_component(_lexxos_fatfs_override
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../fatfs/lexxos_ffconf_override.h" ABSOLUTE)
    if (MSVC)
        target_compile_options(${target} ${visibility} "/FI${_lexxos_fatfs_override}")
    else ()
        target_compile_options(${target} ${visibility}
                "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include \"${_lexxos_fatfs_override}\">")
    endif ()
endfunction()
