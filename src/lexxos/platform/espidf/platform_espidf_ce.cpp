#include <esp_rom_serial_output.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lexxos/platform/espidf/detail/espidf_ce_analog_pwm.h>
#include <lexxos/platform/espidf/detail/espidf_ce_dac.h>
#include <lexxos/platform/espidf/detail/espidf_ce_i2c.h>
#include <lexxos/platform/espidf/detail/espidf_ce_spi.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::platform {

void initialize() {
	// FreeRTOS и UART0 уже подготовлены ESP-IDF до app_main().
	// Устройства приложения инициализируются явно после bootstrap.
}

LegacyBoardDescriptor const& board() {
	// Библиотека знает только целевой чип. Периферию и плату внедряет приложение.
	static LegacyBoardDescriptor const descriptor{
	    .boardId = "esp32",
	    .boardName = "Generic ESP32",
	    .vendor = "Espressif",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32",
	};
	return descriptor;
}

void sleepMs(std::uint32_t milliseconds) {
	if (milliseconds == 0) {
		taskYIELD();
		return;
	}

	auto const ticks = (static_cast<std::uint64_t>(milliseconds) * configTICK_RATE_HZ + 999) / 1000;
	vTaskDelay(static_cast<TickType_t>(ticks < portMAX_DELAY ? ticks : portMAX_DELAY - 1));
}

std::uint32_t uptimeMs() {
	return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

void writeConsole(std::string_view text) {
	// Вывод через ROM console: без RX-буфера, терминала и файлового интерфейса LexxOS.
	for (unsigned char byte : text) {
		(void) esp_rom_output_tx_one_char(byte);
	}
}

void reboot() {
	esp_restart();
}

PlatformServices const& services() {
	static constexpr PlatformServices services{
	    .lifecycle = {.initialize = initialize},
	    .board = {.current = board},
	    .console = {.write = writeConsole},
	    .clock = {.sleepMs = sleepMs, .uptimeMs = uptimeMs},
	    .gpio = {
	                  .configureInput = configureGpioInput,
	                  .configureOutput = configureGpioOutput,
	                  .read = readGpio,
	                  .write = writeGpio,
	                  .configureInterrupt = configureGpioInterrupt,
	                  .disableInterrupt = disableGpioInterrupt,
	                  },
	    .power = {.reboot = reboot},
	    .createI2cBus = espidf::createCeI2cBus,
	    .createSpiController = espidf::createCeSpiController,
	    .createAnalogInput = espidf::createCeAnalogInput,
	    .createPwmOutput = espidf::createCePwmOutput,
	    .createAnalogOutput = espidf::createCeAnalogOutput,
	};
	return services;
}

} // namespace lexxos::platform
