#include <cstdio>

#include "board.h"

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/devices/bus/i2c_bus.h>

extern "C" void app_main() {
	auto const& platform = i2c_sensor::platform();
	static lexxos::core::CoreSystem system{platform};
	auto const print = platform.console.write;
	print("LexxCE: kernel loading\r\n");
	if (!lexxos::core::bootstrap(system)) {
		print("[error] board initialization failed; check I2C configuration\r\n");
		return;
	}

	auto* bus = system.devices().i2cBus("sensors");
	if (!bus) {
		print("[error] missing I2C bus 'sensors'\r\n");
		return;
	}

	auto const& pins = platform.board.current().i2cBuses.front().pins;
	char line[100]{};
	std::snprintf(line, sizeof(line), "kernel ready; sensors SDA=%d SCL=%d %lu Hz\r\n",
	    pins.sda.number, pins.scl.number, static_cast<unsigned long>(pins.baudRate));
	print(line);
	unsigned found = 0;
	for (unsigned address = 0x08; address <= 0x77; ++address) {
		if (!bus->probe(address)) {
			continue;
		}

		++found;
		std::snprintf(line, sizeof(line), "ACK 0x%02X\r\n", address);
		print(line);
		if (address == 0x76 || address == 0x77) {
			constexpr std::uint8_t idRegister = 0xD0;
			std::uint8_t id{};
			if (bus->writeRead(address, &idRegister, 1, &id, 1)) {
				std::snprintf(line, sizeof(line), "0x%02X chip_id=0x%02X%s\r\n",
				    address, static_cast<unsigned>(id), id == 0x60 ? " BME280" : "");
				print(line);
			} else {
				print("[error] chip ID read failed\r\n");
			}
		}
	}

	std::snprintf(line, sizeof(line), "scan complete: %u responding addresses\r\n", found);
	print(line);
	if (!found) {
		print("Check SDA/SCL, supply and pull-ups.\r\n");
	}
}
