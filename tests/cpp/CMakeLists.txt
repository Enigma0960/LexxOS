include("${PROJECT_SOURCE_DIR}/config/cmake/MemoryProfiles.cmake")
include("${PROJECT_SOURCE_DIR}/config/cmake/lexxos_core_sources.cmake")

add_executable(lexxos_board_identity_tests board_identity_tests.cpp)
target_include_directories(lexxos_board_identity_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_board_identity_tests PRIVATE cxx_std_20)
add_test(NAME lexxos_board_identity_tests COMMAND lexxos_board_identity_tests)

add_executable(lexxos_dev_pico_board_tests dev_pico_board_tests.cpp)
target_include_directories(lexxos_dev_pico_board_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_dev_pico_board_tests PRIVATE cxx_std_20)
add_test(NAME lexxos_dev_pico_board_tests COMMAND lexxos_dev_pico_board_tests)

add_executable(lexxos_irq_contract_tests irq_contract_tests.cpp)
target_include_directories(lexxos_irq_contract_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_irq_contract_tests PRIVATE cxx_std_20)

add_executable(lexxos_rk3506_gic_irq_tests
        rk3506_gic_irq_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/gic_irq_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/detail/irq_update_guard.cpp)
target_include_directories(lexxos_rk3506_gic_irq_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_gic_irq_tests PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(lexxos_rk3506_gic_irq_tests PRIVATE /UNDEBUG)
else ()
    target_compile_options(lexxos_rk3506_gic_irq_tests PRIVATE -UNDEBUG)
endif ()

add_executable(lexxos_rk3506_pmu_profile_tests rk3506_pmu_profile_tests.cpp)
target_include_directories(lexxos_rk3506_pmu_profile_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_pmu_profile_tests PRIVATE cxx_std_20)

add_executable(lexxos_rk3506_dma_irq_wait_tests rk3506_dma_irq_wait_tests.cpp)
target_include_directories(lexxos_rk3506_dma_irq_wait_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_dma_irq_wait_tests PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(lexxos_rk3506_dma_irq_wait_tests PRIVATE /UNDEBUG)
else ()
    target_compile_options(lexxos_rk3506_dma_irq_wait_tests PRIVATE -UNDEBUG)
endif ()

add_executable(lexxos_rk3506_gic_dma_irq_tests
        rk3506_gic_dma_irq_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/gic_irq_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/detail/irq_update_guard.cpp)
target_include_directories(lexxos_rk3506_gic_dma_irq_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_gic_dma_irq_tests PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(lexxos_rk3506_gic_dma_irq_tests PRIVATE /UNDEBUG)
else ()
    target_compile_options(lexxos_rk3506_gic_dma_irq_tests PRIVATE -UNDEBUG)
endif ()

if (MSVC)
    target_compile_options(lexxos_irq_contract_tests PRIVATE /UNDEBUG)
else ()
    target_compile_options(lexxos_irq_contract_tests PRIVATE -UNDEBUG)
endif ()

add_executable(lexxos_irq_manager_tests irq_manager_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/detail/irq_update_guard.cpp)
target_include_directories(lexxos_irq_manager_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_irq_manager_tests PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(lexxos_irq_manager_tests PRIVATE /UNDEBUG)
else ()
    target_compile_options(lexxos_irq_manager_tests PRIVATE -UNDEBUG)
endif ()

add_executable(lexxos_lyra_runtime_composition_tests
        lyra_runtime_composition_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_runtime_composition.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_service_state.cpp)
target_include_directories(lexxos_lyra_runtime_composition_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lyra_runtime_composition_tests PRIVATE cxx_std_20)

add_executable(lexxos_lyra_device_composition_tests
        lyra_device_composition_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_device_composition.cpp)
target_include_directories(lexxos_lyra_device_composition_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lyra_device_composition_tests PRIVATE cxx_std_20)

# Check target-specific profile resolution without requiring Pico SDK / ESP-IDF.
function(lexxos_add_sdk_memory_profile_test name)
    add_executable(lexxos_sdk_memory_${name}_tests sdk_memory_profiles_tests.cpp)
    target_include_directories(lexxos_sdk_memory_${name}_tests PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/config)
    target_compile_features(lexxos_sdk_memory_${name}_tests PRIVATE cxx_std_20)
    cmake_parse_arguments(PROFILE "" "CHIP;PLATFORM;BOARD" "" ${ARGN})
    set(_board_args)
    if (PROFILE_BOARD)
        set(_board_args BOARD "${PROFILE_BOARD}")
    endif ()
    lexxos_attach_memory_profile(lexxos_sdk_memory_${name}_tests
            CHIP "${PROFILE_CHIP}" PLATFORM "${PROFILE_PLATFORM}" ${_board_args})
    target_compile_definitions(lexxos_sdk_memory_${name}_tests PRIVATE ${PROFILE_UNPARSED_ARGUMENTS})
endfunction()

lexxos_add_sdk_memory_profile_test(rp2040
        CHIP rp2040 PLATFORM pico
        LEXXOS_PLATFORM_PICO=1 LEXXOS_KERNEL_FREERTOS=1)
lexxos_add_sdk_memory_profile_test(rp2350
        CHIP rp2350 PLATFORM pico
        LEXXOS_PLATFORM_PICO=1 LEXXOS_KERNEL_FREERTOS=1 PICO_RP2350=1)
lexxos_add_sdk_memory_profile_test(picocalc_pico2w
        CHIP rp2350 PLATFORM pico
        BOARD picocalc_pico2w
        LEXXOS_PLATFORM_PICO=1 LEXXOS_KERNEL_FREERTOS=1 PICO_RP2350=1)
lexxos_add_sdk_memory_profile_test(rp2040_heap_96k
        CHIP rp2040 PLATFORM pico
        LEXXOS_PLATFORM_PICO=1 LEXXOS_KERNEL_FREERTOS=1 LEXXOS_PICO_FREERTOS_HEAP_KB=96)
lexxos_add_sdk_memory_profile_test(picocalc_pico_w_heap_88k
        CHIP rp2040 PLATFORM pico
        LEXXOS_PLATFORM_PICO=1 LEXXOS_KERNEL_FREERTOS=1 LEXXOS_PICO_FREERTOS_HEAP_KB=88)
lexxos_add_sdk_memory_profile_test(rp2040_no_freertos
        CHIP rp2040 PLATFORM pico)
lexxos_add_sdk_memory_profile_test(esp32
        CHIP esp32 PLATFORM espidf)
lexxos_add_sdk_memory_profile_test(esp32s3
        CHIP esp32s3 PLATFORM espidf
        BOARD espidf_lcd)
lexxos_add_sdk_memory_profile_test(esp32p4
        CHIP esp32p4 PLATFORM espidf
        BOARD espidf_lcd)

add_executable(lexxos_memory_profiles_tests memory_profiles_tests.cpp)
target_include_directories(lexxos_memory_profiles_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/config)

target_compile_features(lexxos_memory_profiles_tests PRIVATE cxx_std_20)
lexxos_attach_memory_profile(lexxos_memory_profiles_tests CHIP rk3506g2 PLATFORM rk3506 BOARD picocalc_lyra)

add_executable(lexxos_freertos_lazy_mutex_tests freertos_lazy_mutex_tests.cpp)
target_include_directories(lexxos_freertos_lazy_mutex_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_scheduler_stub ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_freertos_lazy_mutex_tests PRIVATE cxx_std_20)

# Контракты CE не подключают shell, VFS, ProgramManager или Standard runtime stubs.
add_executable(lexxos_core_contract_tests core_contract_tests.cpp
        ${LEXXOS_CORE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp)
target_include_directories(lexxos_core_contract_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_core_contract_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_core_contract_tests PRIVATE LEXXOS_PLATFORM_HOST=1)
target_link_libraries(lexxos_core_contract_tests PRIVATE Threads::Threads)

add_executable(lexxos_digital_input_tests digital_input_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input/digital_input.cpp)
target_include_directories(lexxos_digital_input_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_digital_input_tests PRIVATE cxx_std_20)

add_executable(lexxos_digital_output_tests digital_output_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/output/digital_output.cpp)
target_include_directories(lexxos_digital_output_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_digital_output_tests PRIVATE cxx_std_20)

add_executable(lexxos_board_gpio_tests board_gpio_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/board_gpio.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/detail/device_registry_critical_section.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/motion_sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_resource_lock.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input/digital_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/output/digital_output.cpp)
target_include_directories(lexxos_board_gpio_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_board_gpio_tests PRIVATE cxx_std_20)

add_executable(lexxos_ce_blink_tests ce_blink_tests.cpp
        ${PROJECT_SOURCE_DIR}/ce/examples/blink/main/blink_application.cpp
        ${LEXXOS_CORE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp)
target_include_directories(lexxos_ce_blink_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/ce/examples/blink/main)
target_compile_features(lexxos_ce_blink_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ce_blink_tests PRIVATE LEXXOS_PLATFORM_HOST=1 LEXXOS_EDITION_CE=1)
target_link_libraries(lexxos_ce_blink_tests PRIVATE Threads::Threads)

add_executable(lexxos_ce_button_led_tests ce_button_led_tests.cpp
        ${PROJECT_SOURCE_DIR}/ce/examples/button_led/main/app_main.cpp
        ${LEXXOS_CORE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp)
target_include_directories(lexxos_ce_button_led_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/ce/examples/button_led/main)
target_compile_features(lexxos_ce_button_led_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ce_button_led_tests PRIVATE LEXXOS_PLATFORM_HOST=1 LEXXOS_EDITION_CE=1)
target_link_libraries(lexxos_ce_button_led_tests PRIVATE Threads::Threads)

add_executable(lexxos_ce_i2c_tests ce_i2c_tests.cpp
        ${PROJECT_SOURCE_DIR}/ce/examples/i2c_sensor/main/app_main.cpp
        ${LEXXOS_CORE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp)
target_include_directories(lexxos_ce_i2c_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/ce/examples/i2c_sensor/main)
target_compile_features(lexxos_ce_i2c_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ce_i2c_tests PRIVATE LEXXOS_PLATFORM_HOST=1 LEXXOS_EDITION_CE=1)
target_link_libraries(lexxos_ce_i2c_tests PRIVATE Threads::Threads)

foreach (idf_cores IN ITEMS 1 2)
    add_executable(lexxos_espidf_scheduler_${idf_cores}core_tests espidf_scheduler_tests.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/espidf/scheduler_espidf.cpp)
    target_include_directories(lexxos_espidf_scheduler_${idf_cores}core_tests PRIVATE
            espidf_scheduler_stub ${PROJECT_SOURCE_DIR}/src)
    target_compile_features(lexxos_espidf_scheduler_${idf_cores}core_tests PRIVATE cxx_std_20)
    target_compile_definitions(lexxos_espidf_scheduler_${idf_cores}core_tests PRIVATE
            LEXXOS_PLATFORM_ESPIDF=1 LEXXOS_KERNEL_ESPIDF=1 CONFIG_FREERTOS_UNICORE=$<STREQUAL:${idf_cores},1>)
    target_link_libraries(lexxos_espidf_scheduler_${idf_cores}core_tests PRIVATE Threads::Threads)
endforeach ()

add_executable(lexxos_espidf_event_bus_tests espidf_event_bus_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp)
target_include_directories(lexxos_espidf_event_bus_tests PRIVATE espidf_scheduler_stub ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_espidf_event_bus_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_espidf_event_bus_tests PRIVATE LEXXOS_PLATFORM_ESPIDF=1 LEXXOS_KERNEL_ESPIDF=1)
target_link_libraries(lexxos_espidf_event_bus_tests PRIVATE Threads::Threads)

add_executable(lexxos_freertos_scheduler_tests freertos_scheduler_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/freertos/scheduler_freertos.cpp)
target_include_directories(lexxos_freertos_scheduler_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_scheduler_stub ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_freertos_scheduler_tests PRIVATE cxx_std_20)

add_executable(lexxos_sd_dma_owner_tests sd_dma_owner_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_dma_pl330.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_dma.cpp)
target_include_directories(lexxos_sd_dma_owner_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sd_dma_owner_tests PRIVATE cxx_std_20)

add_executable(lexxos_sd_protocol_tests sd_protocol_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/sd_protocol.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/native_sd_protocol_io.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_transport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_media_hooks.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_spi.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_protocol.cpp)
target_include_directories(lexxos_sd_protocol_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sd_protocol_tests PRIVATE cxx_std_20)

add_executable(lexxos_sd_gpio_tests sd_gpio_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/sd_gpio.cpp)
target_include_directories(lexxos_sd_gpio_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sd_gpio_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_sd_gpio_tests PRIVATE LEXXOS_RK3506_SD_GPIO_TEST=1)

add_executable(lexxos_sd_spi_controller_tests
        sd_spi_controller_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/sd_spi_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_spi1_hw.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_sd_spi.cpp)
target_include_directories(lexxos_sd_spi_controller_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sd_spi_controller_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_sd_spi_controller_tests PRIVATE LEXXOS_RK3506_SD_SPI_TEST=1)

add_executable(lexxos_lcd_spi_controller_tests
        lcd_spi_controller_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_spi_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_spi0.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_lcd_spi.cpp)
target_include_directories(lexxos_lcd_spi_controller_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lcd_spi_controller_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_spi_controller_tests PRIVATE LEXXOS_RK3506_LCD_SPI_TEST=1)

foreach (i2c_mode IN ITEMS hardware software)
    set(i2c_owner_target lexxos_lyra_i2c_${i2c_mode}_owner_tests)
    add_executable(${i2c_owner_target}
            lyra_i2c_owner_tests.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_i2c_bus.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_i2c_bus.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/rk3506_i2c_bus.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/software_i2c_bus.cpp
            ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp)
    target_include_directories(${i2c_owner_target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_features(${i2c_owner_target} PRIVATE cxx_std_20)
    if (i2c_mode STREQUAL "software")
        target_compile_definitions(${i2c_owner_target} PRIVATE LEXXOS_RK3506_I2C_SOFTWARE=1)
    endif ()
endforeach ()

add_executable(lexxos_rk3506_gpio_output_tests
        rk3506_gpio_output_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/gpio_output.cpp)
target_include_directories(lexxos_rk3506_gpio_output_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_gpio_output_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_rk3506_gpio_output_tests PRIVATE LEXXOS_RK3506_GPIO_TEST=1)

add_executable(lexxos_lyra_product_tests lyra_product_tests.cpp)
target_include_directories(lexxos_lyra_product_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lyra_product_tests PRIVATE cxx_std_20)

add_executable(lexxos_picocalc_lyra_board_tests picocalc_lyra_board_tests.cpp)
target_include_directories(lexxos_picocalc_lyra_board_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_picocalc_lyra_board_tests PRIVATE cxx_std_20)

add_executable(lexxos_i2c_backend_tests
        i2c_backend_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/software_i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/rk3506_i2c_bus.cpp)
target_include_directories(lexxos_i2c_backend_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_i2c_backend_tests PRIVATE cxx_std_20)

add_executable(lexxos_device_access_tests
        device_access_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/detail/device_registry_critical_section.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/motion_sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_resource_lock.cpp)
target_include_directories(lexxos_device_access_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/config)
target_compile_features(lexxos_device_access_tests PRIVATE cxx_std_20)

find_package(Threads REQUIRED)
add_executable(lexxos_spi_transaction_tests
        spi_transaction_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_transfer.cpp)
target_include_directories(lexxos_spi_transaction_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_spi_transaction_tests PRIVATE cxx_std_20)
target_link_libraries(lexxos_spi_transaction_tests PRIVATE Threads::Threads)

# Synchronization backends and priority-aware queue handoff are exercised
# without a physical RTOS port, plus a FreeRTOS API mock.
add_executable(lexxos_synchronization_host_tests
        synchronization_host_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp)
target_include_directories(lexxos_synchronization_host_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_synchronization_host_tests PRIVATE cxx_std_20)
target_link_libraries(lexxos_synchronization_host_tests PRIVATE Threads::Threads)

add_executable(lexxos_freertos_synchronization_tests
        freertos_synchronization_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/freertos/synchronization_freertos.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/freertos/event_freertos.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/freertos/mutex_freertos.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/freertos/semaphore_freertos.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp)
target_include_directories(lexxos_freertos_synchronization_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_scheduler_stub ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_freertos_synchronization_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_freertos_synchronization_tests PRIVATE
        LEXXOS_PLATFORM_RK3506=1 LEXXOS_KERNEL_FREERTOS=1)
target_link_libraries(lexxos_freertos_synchronization_tests PRIVATE Threads::Threads)

target_link_libraries(lexxos_device_access_tests PRIVATE Threads::Threads)
if (NOT MSVC)
    target_compile_options(lexxos_device_access_tests PRIVATE -UNDEBUG)
endif ()

add_executable(lexxos_rk3506_southbridge_timing_tests
        rk3506_southbridge_timing_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/picocalc/picocalc_southbridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rtc/date_time.cpp)
target_include_directories(lexxos_rk3506_southbridge_timing_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_southbridge_timing_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_rk3506_southbridge_timing_tests PRIVATE LEXXOS_PLATFORM_RK3506=1)

add_executable(lexxos_block_read_benchmark_tests
        block_read_benchmark_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/storage/detail/block_read_benchmark.cpp)
target_include_directories(lexxos_block_read_benchmark_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_block_read_benchmark_tests PRIVATE cxx_std_20)

add_executable(lexxos_rk3506_storage_tests
        rk3506_storage_contract_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_service_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_storage_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/native_picosd_transport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_diagnostic_session.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_media_hooks.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/fs/fat/fat_mount_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/storage/sd_media_transport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/fs/fat/system_fat_mount_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_storage_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_spi_queues.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/storage/sd_card_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picosd_block_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/rk3506/picosd_mount_policy.cpp)
target_include_directories(lexxos_rk3506_storage_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_storage_tests PRIVATE cxx_std_20)

add_executable(lexxos_contract_tests
        program_arguments_tests.cpp
        console_input_tests.cpp
        program_input_tests.cpp
        lifecycle_event_bus_tests.cpp
        runtime_profile_contract_tests.cpp
        picocalc_keyboard_layout_tests.cpp
        picocalc_southbridge_contract_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/program_input_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input/picocalc_keyboard_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/picocalc/picocalc_southbridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rtc/date_time.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
)

add_executable(lexxos_system_contract_tests
        operating_system_contract_tests.cpp
        sdmmc_command_tests.cpp
        board_device_providers_tests.cpp
        info_program_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/common/program_value_support.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/info_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/file_info.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/lexx_info.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/shortcut_info.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/info_output.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/manifest_printer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shortcuts/shortcut.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/board_device_providers.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/detail/host_device_providers.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/detail/legacy_device_providers.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/board_gpio.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input/digital_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/output/digital_output.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/raw_console_mode_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/detail/ymodem_program_support.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/storage/rk3506/sdmmc_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/program_input_queue.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/detail/device_registry_critical_section.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/motion_sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_resource_lock.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/app_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/operating_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/runtime/capabilities.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_memory_arena.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_supervisor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/read_only_filesystem.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_gameboy_settings_tests
        gameboy_settings_tests.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/gameboy/detail/gameboy_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/apu.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge_header.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/read_only_filesystem.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_vfs_file_string_tests
        vfs_file_string_tests.cpp
        resource_parser_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/external_program/lexx_resources.cpp
        support/minimal_runtime_stubs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/read_only_filesystem.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)
add_executable(lexxos_lcd_console_renderer_tests
        lcd_console_renderer_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/runtime/capabilities.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_renderer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_text_renderer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_font.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_character_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_csi_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_hardware_scroll.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_screen_buffer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_status_bar_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_terminal_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_viewport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_icon.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_battery_icon.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_wifi_icon.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_status_bar_renderer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/detail/device_registry_critical_section.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/motion_sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_resource_lock.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp)
target_include_directories(lexxos_lcd_console_renderer_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_scheduler_stub ${PROJECT_SOURCE_DIR}/src ${LEXXOS_GENERATED_INCLUDE_DIR})
target_compile_features(lexxos_lcd_console_renderer_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_console_renderer_tests PRIVATE
        LEXXOS_KERNEL_FREERTOS=1 LEXXOS_RK3506_FREERTOS_PERIPHERALS=1 LEXXOS_PLATFORM_RK3506=1)

add_executable(lexxos_lcd_console_viewport_tests
        lcd_console_viewport_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_csi_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_screen_buffer.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_terminal_state.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/picocalc_lcd_console_viewport.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
)

add_executable(lexxos_services_contract_tests
        platform_services_contract_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_mirror.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/scoped_console_mirror_mode.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/scoped_console_mirror_suspension.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/console_output.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/program_input_service.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/null_console_mirror_sink.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/detail/program_input_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/move_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/fs/rename_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/common/pretty_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/common/pretty_table_row.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/apps/storage/block_devices_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/audio/audio_registry.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/audio/pico_pwm_audio_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/audio/pico_pwm_pcm_audio_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/detail/device_registry_critical_section.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/sensors/motion_sensor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/device_resource_lock.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/gamepad_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/input_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/pointing_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge_header.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/gameboy/cartridge.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/null_input_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/fixed_buffer_input_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/file_input_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/console_output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/null_output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/fixed_buffer_output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/io/file_output_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/lifecycle/event_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/app_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/core/core_context.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/os/operating_system.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/processor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/runtime/capabilities.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/arguments.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_manager.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_memory_arena.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/program/program_supervisor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/command_registry.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shortcuts/shortcut.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_config.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_completion.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_escape_sequence_parser.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_line_editor.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_history_policy.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_redirection_parser.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_mount_table.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_path.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_pseudo_files.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_status.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/detail/vfs_tmpfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/read_only_filesystem.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/vfs/vfs_file_handle.cpp
)

target_include_directories(lexxos_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_include_directories(lexxos_system_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_vfs_file_string_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_gameboy_settings_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_include_directories(lexxos_lcd_console_viewport_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_include_directories(lexxos_services_contract_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${LEXXOS_GENERATED_INCLUDE_DIR}
)

target_compile_features(lexxos_contract_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_system_contract_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_vfs_file_string_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_gameboy_settings_tests PRIVATE cxx_std_20)
target_compile_features(lexxos_lcd_console_viewport_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_console_viewport_tests PRIVATE LEXXOS_RK3506_FREERTOS_PERIPHERALS=1)
target_compile_features(lexxos_services_contract_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_services_contract_tests PRIVATE LEXXOS_TEST_FORCE_NULL_CONSOLE_MIRROR=1)

add_executable(lexxos_shell_history_policy_tests
        shell_history_policy_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/shell/shell_history_policy.cpp)
target_include_directories(lexxos_shell_history_policy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_shell_history_policy_tests PRIVATE cxx_std_20)

# Exercise guest game logic natively; assertions must stay enabled in release builds.
foreach (game IN ITEMS pacman snake tetris)
    add_executable(lexxos_${game}_tests test_${game}.cpp ${PROJECT_SOURCE_DIR}/apps/${game}/game.cpp)
    target_compile_features(lexxos_${game}_tests PRIVATE cxx_std_20)
    if (MSVC)
        target_compile_options(lexxos_${game}_tests PRIVATE /UNDEBUG)
    else ()
        target_compile_options(lexxos_${game}_tests PRIVATE -UNDEBUG)
    endif ()
endforeach ()

add_executable(lexxos_lyra_ram_reload_tests
        lyra_ram_reload_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_ram_reload_session.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp)
target_include_directories(lexxos_lyra_ram_reload_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lyra_ram_reload_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lyra_ram_reload_tests PRIVATE LEXXOS_PLATFORM_RK3506=1)

add_executable(lexxos_ili9488_pixels_tests ili9488_pixels_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_pixels.cpp)
target_include_directories(lexxos_ili9488_pixels_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_ili9488_pixels_tests PRIVATE cxx_std_20)

add_executable(lexxos_ili9488_init_tests ili9488_init_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_init.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_commands.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_pixels.cpp)
target_include_directories(lexxos_ili9488_init_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_ili9488_init_tests PRIVATE cxx_std_20)

add_executable(lexxos_lcd_transaction_tests
        lcd_transaction_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/detail/ili9488_workspace.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/ili9488_buffers.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/ili9488_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/legacy_ili9488_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/ili9488_display.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_init.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_commands.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_stream.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/display/ili9488_pixels.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/composition/products/lyra_spi_queues.cpp)
target_include_directories(lexxos_lcd_transaction_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${LEXXOS_GENERATED_INCLUDE_DIR})
target_compile_features(lexxos_lcd_transaction_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_transaction_tests PRIVATE LEXXOS_PLATFORM_RK3506=1)

add_executable(lexxos_double_buffered_display_tests
        double_buffered_display_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/double_buffered_display.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/dirty_region_set.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display/detail/display_frame_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/dma_irq_clients.cpp)
target_include_directories(lexxos_double_buffered_display_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_double_buffered_display_tests PRIVATE cxx_std_20)

add_executable(lexxos_random_tests lexx_random_tests.cpp)
target_include_directories(lexxos_random_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/sdk)
target_compile_features(lexxos_random_tests PRIVATE cxx_std_20)
add_test(NAME lexxos_random_tests COMMAND lexxos_random_tests)

add_executable(lexxos_ui_clipping_tests lexx_ui_clipping_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp)
target_include_directories(lexxos_ui_clipping_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_ui_clipping_tests PRIVATE cxx_std_20)
add_test(NAME lexxos_ui_clipping_tests COMMAND lexxos_ui_clipping_tests)

add_executable(lexxos_ui_options_tests lexx_ui_options_tests.cpp)
target_include_directories(lexxos_ui_options_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/sdk)
target_compile_features(lexxos_ui_options_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ui_options_tests PRIVATE LEXXOS_UI_NATIVE=1)
add_test(NAME lexxos_ui_options_tests COMMAND lexxos_ui_options_tests)

add_executable(lexx_animation_timeline_tests lexx_animation_timeline_tests.cpp)
target_include_directories(lexx_animation_timeline_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/sdk)
target_compile_features(lexx_animation_timeline_tests PRIVATE cxx_std_20)
add_test(NAME lexx_animation_timeline_tests COMMAND lexx_animation_timeline_tests)

add_executable(lexxos_ui_tree_tests lexx_ui_tree_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/ui/guest_ui.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/ui/guest_ui_tree_layout.cpp)
target_include_directories(lexxos_ui_tree_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/apps/sdk)
target_compile_features(lexxos_ui_tree_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ui_tree_tests PRIVATE LEXXOS_UI_NATIVE=1)

add_executable(lexxos_ui_managed_tree_tests lexx_ui_managed_tree_tests.cpp)
target_include_directories(lexxos_ui_managed_tree_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/sdk)
target_compile_features(lexxos_ui_managed_tree_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_ui_managed_tree_tests PRIVATE LEXXOS_UI_NATIVE=1)
add_test(NAME lexxos_ui_managed_tree_tests COMMAND lexxos_ui_managed_tree_tests)

add_executable(lexxos_system_ui_tests lexx_system_ui_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/display_device.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/ui/guest_ui.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/ui/guest_ui_tree_layout.cpp)
target_include_directories(lexxos_system_ui_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_system_ui_tests PRIVATE cxx_std_20)

add_executable(lexxos_ui_transport_tests lexx_ui_transport_tests.cpp)
target_include_directories(lexxos_ui_transport_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/apps/sdk ${PROJECT_SOURCE_DIR}/apps)
target_compile_definitions(lexxos_ui_transport_tests PRIVATE LEXXOS_UI_NATIVE=1)
target_compile_features(lexxos_ui_transport_tests PRIVATE cxx_std_20)

add_executable(lexxos_lcd_dma_tests
        lcd_dma_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/picocalc_lcd_dma_pl330.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/rk3506/lcd_dma_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_tx_program.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_debug_port.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/dmac0_controller.cpp)
target_include_directories(lexxos_lcd_dma_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lcd_dma_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_dma_tests PRIVATE LEXXOS_RK3506_LCD_DMA_TEST=1 LEXXOS_RK3506_DMAC0_TEST=1)

add_executable(lexxos_pl330_debug_port_tests
        pl330_debug_port_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_debug_port.cpp)
target_include_directories(lexxos_pl330_debug_port_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_pl330_debug_port_tests PRIVATE cxx_std_20)

add_executable(lexxos_rk3506_dmac0_controller_tests
        rk3506_dmac0_controller_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/dmac0_controller.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/drivers/core/dma/pl330_debug_port.cpp)
target_include_directories(lexxos_rk3506_dmac0_controller_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_rk3506_dmac0_controller_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_rk3506_dmac0_controller_tests PRIVATE LEXXOS_RK3506_DMAC0_TEST=1)

# Exercise the PIO-only policy against the same controller and fake MMIO.
get_target_property(_lcd_dma_test_sources lexxos_lcd_dma_tests SOURCES)
add_executable(lexxos_lcd_dma_freertos_policy_tests ${_lcd_dma_test_sources})
target_include_directories(lexxos_lcd_dma_freertos_policy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_lcd_dma_freertos_policy_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_lcd_dma_freertos_policy_tests PRIVATE
        LEXXOS_RK3506_LCD_DMA_TEST=1 LEXXOS_RK3506_DMAC0_TEST=1 LEXXOS_RK3506_FREERTOS=1)

# Public build presets target the OS executable. CTest must build its own binaries,
# including when a single test is selected in a fresh build directory.
foreach (test_target IN ITEMS
        lexxos_lcd_dma_freertos_policy_tests
        lexxos_rk3506_dmac0_controller_tests
        lexxos_pl330_debug_port_tests
        lexxos_lcd_dma_tests
        lexxos_double_buffered_display_tests
        lexxos_system_ui_tests
        lexxos_ui_tree_tests
        lexxos_ui_transport_tests
        lexxos_ili9488_pixels_tests
        lexxos_ili9488_init_tests
        lexxos_lyra_ram_reload_tests
        lexxos_lcd_transaction_tests
        lexxos_irq_contract_tests
        lexxos_rk3506_gic_irq_tests
        lexxos_rk3506_gic_dma_irq_tests
        lexxos_rk3506_dma_irq_wait_tests
        lexxos_irq_manager_tests
        lexxos_spi_transaction_tests
        lexxos_synchronization_host_tests
        lexxos_freertos_synchronization_tests
        lexxos_device_access_tests
        lexxos_lyra_runtime_composition_tests
        lexxos_lyra_device_composition_tests
        lexxos_memory_profiles_tests
        lexxos_sdk_memory_rp2040_tests
        lexxos_sdk_memory_rp2350_tests
        lexxos_sdk_memory_picocalc_pico2w_tests
        lexxos_sdk_memory_rp2040_heap_96k_tests
        lexxos_sdk_memory_picocalc_pico_w_heap_88k_tests
        lexxos_sdk_memory_rp2040_no_freertos_tests
        lexxos_sdk_memory_esp32_tests
        lexxos_sdk_memory_esp32s3_tests
        lexxos_sdk_memory_esp32p4_tests
        lexxos_sd_protocol_tests
        lexxos_sd_dma_owner_tests
        lexxos_core_contract_tests
        lexxos_ce_blink_tests
        lexxos_ce_button_led_tests
        lexxos_ce_i2c_tests
        lexxos_digital_input_tests
        lexxos_digital_output_tests
        lexxos_board_gpio_tests
        lexxos_espidf_scheduler_1core_tests
        lexxos_espidf_scheduler_2core_tests
        lexxos_espidf_event_bus_tests
        lexxos_freertos_scheduler_tests
        lexxos_freertos_lazy_mutex_tests
        lexxos_sd_gpio_tests
        lexxos_sd_spi_controller_tests
        lexxos_lcd_spi_controller_tests
        lexxos_lyra_i2c_hardware_owner_tests
        lexxos_lyra_i2c_software_owner_tests
        lexxos_rk3506_gpio_output_tests
        lexxos_lyra_product_tests
        lexxos_picocalc_lyra_board_tests
        lexxos_i2c_backend_tests
        lexxos_rk3506_southbridge_timing_tests
        lexxos_block_read_benchmark_tests
        lexxos_rk3506_storage_tests
        lexxos_contract_tests
        lexxos_system_contract_tests
        lexxos_gameboy_settings_tests
        lexxos_vfs_file_string_tests
        lexxos_lcd_console_viewport_tests
        lexxos_lcd_console_renderer_tests
        lexxos_services_contract_tests
        lexxos_shell_history_policy_tests
        lexxos_pacman_tests
        lexxos_snake_tests
        lexxos_tetris_tests)
    add_test(NAME ${test_target}_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --config $<CONFIG> --target ${test_target})
    set_tests_properties(${test_target}_build PROPERTIES
            FIXTURES_SETUP ${test_target}_ready
            RESOURCE_LOCK lexxos_test_build)
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
            FIXTURES_REQUIRED ${test_target}_ready)
endforeach ()

set_tests_properties(lexxos_core_contract_tests lexxos_espidf_scheduler_1core_tests
        lexxos_espidf_scheduler_2core_tests lexxos_espidf_event_bus_tests PROPERTIES TIMEOUT 60)

# The queue uses the same target-specific cooperative/atomic guard as its callers.
foreach (queue_target IN ITEMS
        lexxos_device_access_tests lexxos_spi_transaction_tests
        lexxos_i2c_backend_tests lexxos_rk3506_southbridge_timing_tests
        lexxos_contract_tests lexxos_system_contract_tests lexxos_services_contract_tests)
    target_sources(${queue_target} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/bus_transaction_queue.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/detail/bus_transaction_queue_guard.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/i2c_bus_transaction.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/devices/bus/spi_bus_transaction.cpp)
endforeach ()

# Every fixture linking a standalone queue also needs the host event backend.
# RK3506-tagged fixtures retain the cooperative path at compile time.
foreach (sync_queue_target IN ITEMS
        lexxos_device_access_tests lexxos_spi_transaction_tests
        lexxos_i2c_backend_tests lexxos_rk3506_southbridge_timing_tests
        lexxos_contract_tests lexxos_system_contract_tests lexxos_services_contract_tests
        lexxos_lyra_i2c_hardware_owner_tests lexxos_lyra_i2c_software_owner_tests
        lexxos_rk3506_storage_tests lexxos_lyra_ram_reload_tests
        lexxos_lcd_transaction_tests)
    target_sources(${sync_queue_target} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/synchronization_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/event_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/mutex_host.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/kernel/host/semaphore_host.cpp)
    target_link_libraries(${sync_queue_target} PRIVATE Threads::Threads)
endforeach ()

# Prevent diagnostic subcommands from slipping into an unrelated action block.
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    add_test(NAME lexxos_sd_diag_dma_routing_tests
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test_sd_diag_dma_routing.py")
endif ()

# Компиляция чистого SDMMC backend на host; MMIO-код не исполняется.
add_library(lexxos_sdmmc_backend_compile OBJECT
        ${PROJECT_SOURCE_DIR}/src/lexxos/platform/rockchip/rk3506/sdmmc_probe.cpp)
target_include_directories(lexxos_sdmmc_backend_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sdmmc_backend_compile PRIVATE cxx_std_20)

add_executable(lexxos_sd_hotplug_policy_tests sd_hotplug_policy_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/lexxos/app/sd_hotplug_policy.cpp)
target_include_directories(lexxos_sd_hotplug_policy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_sd_hotplug_policy_tests PRIVATE cxx_std_20)
add_test(NAME lexxos_sd_hotplug_policy_tests COMMAND lexxos_sd_hotplug_policy_tests)

# Definitions remain platform-neutral even when legacy target macros are present.
add_executable(lexxos_runtime_profile_definition_tests runtime_profile_definition_tests.cpp)
target_include_directories(lexxos_runtime_profile_definition_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(lexxos_runtime_profile_definition_tests PRIVATE cxx_std_20)
target_compile_definitions(lexxos_runtime_profile_definition_tests PRIVATE
        LEXXOS_MEMORY_CHIP_RK3506G2=1 LEXXOS_MEMORY_PLATFORM_RK3506=1)
add_test(NAME lexxos_runtime_profile_definition_tests COMMAND lexxos_runtime_profile_definition_tests)
if (Python3_Interpreter_FOUND)
    add_test(NAME lexxos_memory_profile_selection_tests
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/python/test_memory_profile_selection.py")
endif ()
