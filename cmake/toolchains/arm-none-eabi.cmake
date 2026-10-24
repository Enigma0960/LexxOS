set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# CMake re-includes the toolchain file inside internal try_compile() projects.
# Keep the explicitly selected Arm toolchain root available there as well;
# otherwise the scratch project falls back to PATH and fails when the Pico SDK
# toolchain is installed outside PATH.
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LEXXOS_ARM_TOOLCHAIN_PATH)

set(LEXXOS_ARM_TOOLCHAIN_PATH "" CACHE PATH "Path to an Arm GNU Toolchain root, for example ~/.pico-sdk/toolchain/14_2_Rel1")

if (LEXXOS_ARM_TOOLCHAIN_PATH)
    set(_lexxos_arm_toolchain_bin "${LEXXOS_ARM_TOOLCHAIN_PATH}/bin")
else ()
    set(_lexxos_arm_toolchain_bin "")
endif ()

function(_lexxos_find_arm_tool out_var tool_name)
    if (_lexxos_arm_toolchain_bin)
        set(_candidate "${_lexxos_arm_toolchain_bin}/${tool_name}")
        if (EXISTS "${_candidate}")
            set(${out_var} "${_candidate}" PARENT_SCOPE)
            return()
        endif ()
    endif ()

    # During try_compile() CMake may already know the compiler path, while
    # LEXXOS_ARM_TOOLCHAIN_PATH has not yet been restored in the scratch cache.
    # Use the known compiler directory to locate sibling tools.
    if (CMAKE_C_COMPILER)
        get_filename_component(_known_compiler_bin "${CMAKE_C_COMPILER}" DIRECTORY)
        set(_candidate "${_known_compiler_bin}/${tool_name}")
        if (EXISTS "${_candidate}")
            set(${out_var} "${_candidate}" PARENT_SCOPE)
            return()
        endif ()
    endif ()

    # Convenience fallback for a standard Raspberry Pi Pico SDK installation.
    # This keeps the preset usable even when the toolchain bin directory is not
    # exported into PATH.
    if (DEFINED ENV{HOME})
        file(GLOB _pico_toolchain_roots "$ENV{HOME}/.pico-sdk/toolchain/*")
        foreach (_pico_toolchain_root IN LISTS _pico_toolchain_roots)
            set(_candidate "${_pico_toolchain_root}/bin/${tool_name}")
            if (EXISTS "${_candidate}")
                set(${out_var} "${_candidate}" PARENT_SCOPE)
                return()
            endif ()
        endforeach ()
    endif ()

    find_program(_found_${out_var} ${tool_name})
    if (NOT _found_${out_var})
        message(FATAL_ERROR "${tool_name} was not found. Set LEXXOS_ARM_TOOLCHAIN_PATH to your Arm GNU Toolchain root or add the toolchain bin directory to PATH.")
    endif ()
    set(${out_var} "${_found_${out_var}}" PARENT_SCOPE)
endfunction()

_lexxos_find_arm_tool(_lexxos_c_compiler arm-none-eabi-gcc)
_lexxos_find_arm_tool(_lexxos_cxx_compiler arm-none-eabi-g++)
_lexxos_find_arm_tool(_lexxos_asm_compiler arm-none-eabi-gcc)
_lexxos_find_arm_tool(_lexxos_objcopy arm-none-eabi-objcopy)
_lexxos_find_arm_tool(_lexxos_size arm-none-eabi-size)

set(CMAKE_C_COMPILER "${_lexxos_c_compiler}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_lexxos_cxx_compiler}" CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER "${_lexxos_asm_compiler}" CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_OBJCOPY "${_lexxos_objcopy}" CACHE FILEPATH "objcopy" FORCE)
set(CMAKE_SIZE "${_lexxos_size}" CACHE FILEPATH "size" FORCE)
