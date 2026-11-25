#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

namespace {

	bool usesPin(platform::I2cPins const& bus, platform::GpioPin pin) {
		return pin.number == bus.sda.number || pin.number == bus.scl.number;
	}

	bool validTable(platform::LegacyBoardDescriptor const& board) {
		if (board.userGpio.count > platform::maxBoardUserGpioPins) {
			return false;
		}

		for (std::size_t i = 0; i < board.i2cBuses.size(); ++i) {
			auto const& [name, pins] = board.i2cBuses[i];
			if (name.empty() || !pins.isValid() || pins.index < 0 || pins.baudRate == 0 || pins.sda.number == pins.scl.number || usesPin(pins, board.statusLed) || usesPin(pins, board.debugUart.tx) || usesPin(pins, board.debugUart.rx)) {
				return false;
			}

			for (std::size_t j = 0; j < board.userGpio.count; ++j) {
				if (usesPin(pins, board.userGpio.pins[j].pin)) {
					return false;
				}
			}

			for (auto const& previous : board.i2cBuses.first(i)) {
				if (name == previous.name || previous.pins.index == pins.index || usesPin(pins, previous.pins.sda) || usesPin(pins, previous.pins.scl)) {
					return false;
				}
			}
		}

		return true;
	}

} // namespace

bool DeviceManager::bindBoardI2c(platform::LegacyBoardDescriptor const& board, platform::PlatformServices const& services) {
	auto buses = board.i2cBuses;
	if (buses.empty()) {
		return true;
	}

	if (!services.createI2cBus || i2cBusCount() || buses.size() > maxI2cBuses || !validTable(board)) {
		return false;
	}

	decltype(_boardI2c) pending{};
	for (std::size_t i = 0; i < buses.size(); ++i) {
		pending[i] = services.createI2cBus();
		if (!pending[i] || !pending[i]->initialize(buses[i].pins)) {
			return false;
		}

		pending[i]->setTransactionWaitFunction(services.clock.sleepMs);
	}

	for (std::size_t i = 0; i < buses.size(); ++i) {
		if (!registerI2cBus(buses[i].name, "board I2C master", *pending[i])) {
			_i2cBuses.clear();
			return false;
		}
	}

	_boardI2c = std::move(pending);
	return true;
}

} // namespace lexxos::devices
