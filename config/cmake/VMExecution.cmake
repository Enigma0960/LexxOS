# Portable RV32I VM execution profiles.
# No change to the .lexx executable format or guest ABI.
set(LEXXOS_VM_PROFILE "auto" CACHE STRING
        "VM profile: auto, compact, fast, cached8, cached16")
set_property(CACHE LEXXOS_VM_PROFILE PROPERTY STRINGS
        auto compact fast cached8 cached16)

option(LEXXOS_VM_LOCAL_PC
        "Keep RV32I PC local between instructions in fast VM profiles" ON)


function(lexxos_configure_vm_target target platform)
    set(profile "${LEXXOS_VM_PROFILE}")
    if (profile STREQUAL "auto")
        if (platform STREQUAL "pico" OR platform STREQUAL "stm32f4"
                OR (platform STREQUAL "rk3506"
                AND NOT LEXXOS_RK3506_VM_FAST_LOOP))
            set(profile "compact")
        else ()
            set(profile "fast")
        endif ()
    endif ()

    if (profile STREQUAL "compact")
        set(level 0)
        set(entries 0)
    elseif (profile STREQUAL "fast")
        set(level 1)
        set(entries 0)
    elseif (profile STREQUAL "cached8")
        set(level 2)
        set(entries 8)
    elseif (profile STREQUAL "cached16")
        set(level 2)
        set(entries 16)
    else ()
        message(FATAL_ERROR "Unsupported LEXXOS_VM_PROFILE='${profile}'")
    endif ()

    target_compile_definitions(${target} PRIVATE
            LEXXOS_VM_EXECUTION_PROFILE=${level}
            LEXXOS_VM_DECODE_CACHE_ENTRIES=${entries})

    if (NOT level EQUAL 0)
        target_compile_definitions(${target} PRIVATE
                LEXXOS_VM_FAST_LOOP=1
                LEXXOS_VM_SINGLE_WRITE_ALU=1)
        if (LEXXOS_VM_LOCAL_PC)
            target_compile_definitions(${target} PRIVATE
                    LEXXOS_VM_LOCAL_PC=1)
        endif ()
    endif ()

    message(STATUS "LexxOS VM: ${profile}, decode-cache=${entries}")
endfunction()
