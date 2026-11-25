get_filename_component(LEXXOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(
        SRCS "${LEXXOS_ROOT}/src/lexxos/kernel/cooperative/scheduler_cooperative.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/cooperative/synchronization_cooperative.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/cooperative/event_cooperative.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/cooperative/mutex_cooperative.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/cooperative/semaphore_cooperative.cpp"
        REQUIRES lexxos_ce)
