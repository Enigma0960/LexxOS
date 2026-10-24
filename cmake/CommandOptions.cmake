include_guard(GLOBAL)

foreach(group IN ITEMS SYSTEM UTILITIES ADMIN DIAGNOSTICS APPLICATIONS)
    option(LEXXOS_COMMANDS_${group} "Include ${group} built-in commands" ON)
endforeach()
option(LEXXOS_COMMANDS_TESTS "Include active test and benchmark commands" ${LEXXOS_FEATURE_TEST_COMMANDS})
option(LEXXOS_COMMANDS_EXPERIMENTAL "Include experimental command tools (full only)" ${LEXXOS_FULL_FEATURES_DEFAULT})

function(lexxos_apply_command_policy)
    # Effective values mask cached per-device settings without losing the user's
    # selections when TESTS is enabled again. BUILD_TESTING is independent.
    if (NOT LEXXOS_COMMANDS_TESTS)
        foreach(feature IN ITEMS SD_CARD_TEST_COMMANDS DISPLAY_TEST_COMMANDS KEYBOARD_TEST_COMMANDS)
            set(LEXXOS_FEATURE_${feature} OFF PARENT_SCOPE)
        endforeach()
    endif()
    if (NOT LEXXOS_COMMANDS_EXPERIMENTAL)
        set(LEXXOS_FEATURE_GPIO_ANALYZER OFF PARENT_SCOPE)
    endif()

    # Hardware removal also removes its test command subgroup, even in a full preset.
    foreach(pair IN ITEMS "SD_CARD" "DISPLAY" "KEYBOARD")
        if (NOT LEXXOS_FEATURE_${pair})
            set(LEXXOS_FEATURE_${pair}_TEST_COMMANDS OFF PARENT_SCOPE)
        endif()
    endforeach()
    if (NOT LEXXOS_FEATURE_DISPLAY)
        set(LEXXOS_FEATURE_GPIO_ANALYZER OFF PARENT_SCOPE)
    endif()
endfunction()
