get_filename_component(LEXXOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(
        SRCS "${LEXXOS_ROOT}/src/lexxos/kernel/espidf/scheduler_espidf.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/freertos/synchronization_freertos.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/freertos/event_freertos.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/freertos/mutex_freertos.cpp"
        "${LEXXOS_ROOT}/src/lexxos/kernel/freertos/semaphore_freertos.cpp"
        REQUIRES lexxos_ce freertos)
