#include <cstdio>

#include "board.h"

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/kernel/cooperative/cooperative_loop.h>

namespace {

struct Application {
	lexxos::devices::AnalogInput& input;
	lexxos::devices::PwmOutput& output;
	lexxos::platform::PlatformServices const& platform;
	std::uint32_t lastReportMs{};

	bool stop(char const* reason) {
		platform.console.write(reason);
		if (!output.setDutyPermille(0)) {
			platform.console.write("[error] Не удалось выключить PWM; состояние выхода неизвестно\r\n");
		}
		return false;
	}

	bool tick() {
		std::uint32_t raw{};
		if (!input.readRaw(raw)) {
			return stop("[error] Ошибка чтения АЦП; обработчик остановлен\r\n");
		}

		auto const duty = static_cast<std::uint16_t>(static_cast<std::uint64_t>(raw) * 1000 / input.maxCode());
		if (!output.setDutyPermille(duty)) {
			return stop("[error] Ошибка PWM; обработчик остановлен\r\n");
		}

		auto const now = platform.clock.uptimeMs();
		if (now - lastReportMs >= 1000) {
			lastReportMs = now;
			char line[160]{};
			std::snprintf(line, sizeof(line), "ADC=%lu/%lu; PWM=%u.%u%%\r\n",
			    static_cast<unsigned long>(raw), static_cast<unsigned long>(input.maxCode()),
			    static_cast<unsigned>(duty / 10), static_cast<unsigned>(duty % 10));
			platform.console.write(line);
		}

		return true;
	}
};

} // namespace

extern "C" void app_main() {
	auto const& platform = analog_pwm::platform();
	static lexxos::core::CoreSystem system{platform};
	platform.console.write("LexxCE: загрузка ядра\r\n");
	if (!lexxos::core::bootstrap(system)) {
		platform.console.write("[error] Не удалось инициализировать периферию платы\r\n");
		return;
	}

	auto* input = system.devices().analogInput("knob");
	auto* output = system.devices().pwmOutput("led");
	if (!input || !output || !input->maxCode() || !platform.clock.uptimeMs || !platform.clock.sleepMs) {
		platform.console.write("[error] Отсутствует драйвер или системные часы\r\n");
		return;
	}

	char line[192]{};
	std::snprintf(line, sizeof(line), "Ядро готово; ADC max=%lu, калибровка=%s; PWM=%lu Hz\r\n",
	    static_cast<unsigned long>(input->maxCode()), input->hasCalibration() ? "есть" : "нет",
	    static_cast<unsigned long>(output->frequencyHz()));
	platform.console.write(line);
	Application application{*input, *output, platform, platform.clock.uptimeMs()};
	lexxos::kernel::CooperativeLoop loop;
	if (!loop.add([](void* context) { return static_cast<Application*>(context)->tick(); },
	        &application, 20, platform.clock.uptimeMs())) {
		platform.console.write("[error] Не удалось добавить обработчик\r\n");
		return;
	}
	while (loop.taskCount()) {
		loop.poll(platform.clock.uptimeMs());
		platform.clock.sleepMs(1);
	}
}
