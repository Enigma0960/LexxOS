#include <cstdio>

#include "board.h"

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>

extern "C" void app_main() {
	auto const& platform = spi_devices::platform();
	static lexxos::core::CoreSystem system{platform};
	platform.console.write("LexxCE: kernel loading\r\n");
	if (!lexxos::core::bootstrap(system)) {
		platform.console.write("[error] board SPI initialization failed\r\n");
		return;
	}

	platform.console.write("kernel ready\r\n");
	for (auto const& bus : platform.board.current().spiBuses) {
		for (auto const& config : bus.devices) {
			auto* device = system.devices().spiBus(config.name);
			if (!device || !device->isReady()) {
				platform.console.write("[error] SPI device missing\r\n");
				return;
			}

			char line[128]{};
			std::snprintf(line, sizeof(line), "%.*s: driver ready; CS=%d mode=%u clock=%lu Hz\r\n",
			    static_cast<int>(config.name.size()), config.name.data(), config.cs.number,
			    static_cast<unsigned>(device->clockMode()), static_cast<unsigned long>(config.baudRate));
			platform.console.write(line);
		}
	}
}
