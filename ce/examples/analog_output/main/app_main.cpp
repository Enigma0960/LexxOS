#include <cstdio>

#include "board.h"

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/kernel/cooperative/cooperative_loop.h>

namespace {

struct Application {
	lexxos::devices::AnalogOutput& output;
	lexxos::platform::PlatformServices const& platform;
	unsigned nextLevel{1};

	bool report() {
		std::uint32_t code{};
		if (!output.currentCode(code)) {
			platform.console.write("[error] Состояние ЦАП неизвестно\r\n");
			return false;
		}

		char line[160]{};
		std::snprintf(line, sizeof(line), "DAC: задан код %lu/%lu\r\n",
		    static_cast<unsigned long>(code), static_cast<unsigned long>(output.maxCode()));
		platform.console.write(line);
		return true;
	}

	bool tick() {
		// Пять уровней 0%, 25%, 50%, 75%, 100% диапазона кода, с округлением.
		auto code = static_cast<std::uint32_t>((static_cast<std::uint64_t>(output.maxCode()) * nextLevel + 2) / 4);
		if (!output.writeRaw(code)) {
			platform.console.write("[error] Ошибка записи ЦАП; обработчик остановлен\r\n");
			if (!output.writeRaw(0)) {
				platform.console.write("[error] Не удалось установить нулевой код\r\n");
			}

			return false;
		}

		nextLevel = (nextLevel + 1) % 5;
		return report();
	}
};

} // namespace

extern "C" void app_main() {
	auto const& platform = analog_output::platform();
	static lexxos::core::CoreSystem system{platform};
	platform.console.write("LexxCE: загрузка ядра\r\n");
	if (!lexxos::core::bootstrap(system)) {
		platform.console.write("[error] Не удалось инициализировать периферию платы\r\n");
		return;
	}

	auto* output = system.devices().analogOutput("reference");
	if (!output || !output->isReady() || !platform.clock.uptimeMs || !platform.clock.sleepMs) {
		platform.console.write("[error] Отсутствует драйвер или системные часы\r\n");
		return;
	}

	platform.console.write("Ядро готово; кооперативный цикл ЦАП, шаг 1 секунда\r\n");
	Application application{*output, platform};
	if (!application.report()) {
		return;
	}

	lexxos::kernel::CooperativeLoop loop;
	if (!loop.add([](void* context) { return static_cast<Application*>(context)->tick(); },
	        &application, 1000, platform.clock.uptimeMs())) {
		platform.console.write("[error] Не удалось добавить обработчик\r\n");
		return;
	}
	while (loop.taskCount()) {
		loop.poll(platform.clock.uptimeMs());
		platform.clock.sleepMs(1);
	}
}
