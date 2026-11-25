#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

namespace {

	bool samePin(platform::GpioPin a, platform::GpioPin b) {
		return a.isValid() && b.isValid() && a.number == b.number;
	}

	bool usesPin(platform::NamedSpiBus const& bus, platform::GpioPin pin) {
		if (samePin(bus.pins.sck, pin) || samePin(bus.pins.mosi, pin) || samePin(bus.pins.miso, pin)) {
			return true;
		}

		for (auto const& device : bus.devices) {
			if (samePin(device.cs, pin)) {
				return true;
			}
		}

		return false;
	}

	bool reservedPin(platform::LegacyBoardDescriptor const& board, platform::GpioPin pin) {
		if (samePin(pin, board.statusLed) || samePin(pin, board.debugUart.tx) || samePin(pin, board.debugUart.rx)) {
			return true;
		}

		for (std::size_t i = 0; i < board.userGpio.count; ++i) {
			if (samePin(pin, board.userGpio.pins[i].pin)) {
				return true;
			}
		}

		for (auto const& bus : board.i2cBuses) {
			if (samePin(pin, bus.pins.sda) || samePin(pin, bus.pins.scl)) {
				return true;
			}
		}

		return false;
	}

	bool validTable(platform::LegacyBoardDescriptor const& board) {
		if (board.userGpio.count > platform::maxBoardUserGpioPins) {
			return false;
		}

		std::size_t count = 0;
		for (std::size_t i = 0; i < board.spiBuses.size(); ++i) {
			auto const& bus = board.spiBuses[i];
			auto const& pins = bus.pins;
			if (bus.name.empty() || bus.devices.empty() || pins.index < 0 || !pins.sck.isValid() || (!pins.mosi.isValid() && !pins.miso.isValid()) || pins.mosi.number < -1 || pins.miso.number < -1 || samePin(pins.sck, pins.mosi) || samePin(pins.sck, pins.miso) || samePin(pins.mosi, pins.miso)) {
				return false;
			}

			if (bus.devices.size() > DeviceManager::maxSpiBuses - count) {
				return false;
			}

			count += bus.devices.size();
			for (auto pin : {pins.sck, pins.mosi, pins.miso}) {
				if (reservedPin(board, pin)) {
					return false;
				}

				for (auto const& previous : board.spiBuses.first(i)) {
					if (usesPin(previous, pin)) {
						return false;
					}
				}
			}
			for (auto const& previous : board.spiBuses.first(i)) {
				if (previous.name == bus.name || previous.pins.index == pins.index) {
					return false;
				}
			}

			for (std::size_t j = 0; j < bus.devices.size(); ++j) {
				auto const& device = bus.devices[j];
				if (device.name.empty() || !device.cs.isValid() || !device.baudRate || static_cast<unsigned>(device.mode) > 3 || reservedPin(board, device.cs) || samePin(device.cs, pins.sck) || samePin(device.cs, pins.mosi) || samePin(device.cs, pins.miso)) {
					return false;
				}

				for (auto const& previous : bus.devices.first(j)) {
					if (previous.name == device.name || samePin(previous.cs, device.cs)) {
						return false;
					}
				}

				for (auto const& previous : board.spiBuses.first(i)) {
					if (usesPin(previous, device.cs)) {
						return false;
					}

					for (auto const& other : previous.devices) {
						if (other.name == device.name) {
							return false;
						}
					}
				}
			}
		}

		return true;
	}

} // namespace

bool DeviceManager::bindBoardSpi(platform::LegacyBoardDescriptor const& board, platform::PlatformServices const& services) {
	if (board.spiBuses.empty()) {
		return true;
	}

	if (!services.createSpiController || spiBusCount() || board.spiBuses.size() > maxSpiBuses || !validTable(board)) {
		return false;
	}

	decltype(_boardSpiControllers) controllers{};
	decltype(_boardSpi) endpoints{};
	std::size_t next = 0;
	for (std::size_t i = 0; i < board.spiBuses.size(); ++i) {
		auto const& bus = board.spiBuses[i];
		controllers[i] = services.createSpiController();
		if (!controllers[i] || !controllers[i]->initialize(bus.pins)) {
			return false;
		}

		for (auto const& config : bus.devices) {
			auto& endpoint = endpoints[next++];
			endpoint = controllers[i]->createDevice(config);
			if (!endpoint || !endpoint->isReady()) {
				return false;
			}

			endpoint->setTransactionWaitFunction(services.clock.sleepMs);
		}
	}

	next = 0;
	for (auto const& bus : board.spiBuses) {
		for (auto const& config : bus.devices) {
			if (!registerSpiBus(config.name, bus.name, *endpoints[next++])) {
				_spiBuses.clear();
				return false;
			}
		}
	}

	_boardSpiControllers = std::move(controllers);
	_boardSpi = std::move(endpoints);
	return true;
}

} // namespace lexxos::devices
