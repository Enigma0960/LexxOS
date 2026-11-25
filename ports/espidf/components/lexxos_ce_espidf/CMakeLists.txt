get_filename_component(LEXXOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(
        SRCS "${LEXXOS_ROOT}/src/lexxos/platform/espidf/platform_espidf_ce.cpp"
        "${LEXXOS_ROOT}/src/lexxos/platform/espidf/detail/espidf_gpio.cpp"
        "${LEXXOS_ROOT}/src/lexxos/platform/espidf/detail/espidf_ce_i2c.cpp"
        "${LEXXOS_ROOT}/src/lexxos/platform/espidf/detail/espidf_ce_spi.cpp"
        "${LEXXOS_ROOT}/src/lexxos/platform/espidf/detail/espidf_ce_analog_pwm.cpp"
        "${LEXXOS_ROOT}/src/lexxos/platform/espidf/detail/espidf_ce_dac.cpp"
        REQUIRES lexxos_ce freertos esp_driver_gpio esp_driver_i2c esp_driver_spi esp_adc esp_driver_ledc esp_driver_dac esp_timer esp_system esp_rom)
