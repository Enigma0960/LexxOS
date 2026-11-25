#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

namespace {

	bool samePin(platform::GpioPin a, platform::GpioPin b) {
		return a.isValid() && b.isValid() && a.number == b.number;
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

		for (auto const& bus : board.spiBuses) {
			if (samePin(pin, bus.pins.sck) || samePin(pin, bus.pins.mosi) || samePin(pin, bus.pins.miso)) {
				return true;
			}

			for (auto const& device : bus.devices) {
				if (samePin(pin, device.cs)) {
					return true;
				}
			}
		}

		return false;
	}

	bool validTable(platform::LegacyBoardDescriptor const& board) {
		if (board.userGpio.count > platform::maxBoardUserGpioPins) {
			return false;
		}

		for (std::size_t i = 0; i < board.analogInputs.size(); ++i) {
			auto const& input = board.analogInputs[i];
			if (input.name.empty() || !input.pin.isValid() || !input.resolutionBits || input.resolutionBits > 32 || static_cast<unsigned>(input.attenuation) > static_cast<unsigned>(platform::AnalogAttenuation::Db12) || reservedPin(board, input.pin)) {
				return false;
			}

			for (auto const& previous : board.analogInputs.first(i)) {
				if (input.name == previous.name || samePin(input.pin, previous.pin)) {
					return false;
				}
			}
		}

		for (std::size_t i = 0; i < board.analogOutputs.size(); ++i) {
			auto const& output = board.analogOutputs[i];
			if (output.name.empty() || !output.pin.isValid() || !output.resolutionBits || output.resolutionBits > 32 || reservedPin(board, output.pin)) {
				return false;
			}

			// Расчёт в 64 битах сохраняет корректность для разрешения 32 бита.
			auto const maximum = (std::uint64_t{1} << output.resolutionBits) - 1;
			if (output.initialCode > maximum) {
				return false;
			}

			for (auto const& input : board.analogInputs) {
				if (output.name == input.name || samePin(output.pin, input.pin)) {
					return false;
				}
			}

			for (auto const& previous : board.analogOutputs.first(i)) {
				if (output.name == previous.name || samePin(output.pin, previous.pin)) {
					return false;
				}
			}
		}

		for (std::size_t i = 0; i < board.pwmOutputs.size(); ++i) {
			auto const& output = board.pwmOutputs[i];
			if (output.name.empty() || !output.pin.isValid() || !output.frequencyHz || !output.resolutionBits || output.resolutionBits > 32 || output.initialDutyPermille > 1000 || (output.activeLevel != platform::GpioActiveLevel::High && output.activeLevel != platform::GpioActiveLevel::Low) || reservedPin(board, output.pin)) {
				return false;
			}

			for (auto const& input : board.analogInputs) {
				if (samePin(input.pin, output.pin) || input.name == output.name) {
					return false;
				}
			}

			for (auto const& analog : board.analogOutputs) {
				if (output.name == analog.name || samePin(output.pin, analog.pin)) {
					return false;
				}
			}

			for (auto const& previous : board.pwmOutputs.first(i)) {
				if (output.name == previous.name || samePin(output.pin, previous.pin) || output.channel == previous.channel) {
					return false;
				}

				if (output.timer == previous.timer && (output.frequencyHz != previous.frequencyHz || output.resolutionBits != previous.resolutionBits)) {
					return false;
				}
			}
		}

		return true;
	}

} // namespace

bool DeviceManager::bindBoardAnalogPwm(platform::LegacyBoardDescriptor const& board, platform::PlatformServices const& services) {
	if (board.analogInputs.empty() && board.pwmOutputs.empty() && board.analogOutputs.empty()) {
		return true;
	}

	if (board.analogInputs.size() > maxAnalogInputs || board.pwmOutputs.size() > maxPwmOutputs || board.analogOutputs.size() > maxAnalogOutputs || _boardAnalogInputs[0].driver || _boardPwmOutputs[0].driver || _boardAnalogOutputs[0].driver || (!board.analogInputs.empty() && !services.createAnalogInput) || (!board.pwmOutputs.empty() && !services.createPwmOutput) || (!board.analogOutputs.empty() && !services.createAnalogOutput) || !validTable(board)) {
		return false;
	}

	decltype(_boardAnalogInputs) inputs{};
	decltype(_boardPwmOutputs) outputs{};
	decltype(_boardAnalogOutputs) analogOutputs{};
	for (std::size_t i = 0; i < board.analogInputs.size(); ++i) {
		auto const& config = board.analogInputs[i];
		inputs[i] = {config.name, services.createAnalogInput(config)};
		if (!inputs[i].driver || !inputs[i].driver->isReady()) {
			return false;
		}
	}

	for (std::size_t i = 0; i < board.pwmOutputs.size(); ++i) {
		auto const& config = board.pwmOutputs[i];
		outputs[i] = {config.name, services.createPwmOutput(config)};
		if (!outputs[i].driver || !outputs[i].driver->isReady()) {
			return false;
		}
	}

	for (std::size_t i = 0; i < board.analogOutputs.size(); ++i) {
		auto const& config = board.analogOutputs[i];
		analogOutputs[i] = {config.name, services.createAnalogOutput(config)};
		if (!analogOutputs[i].driver || !analogOutputs[i].driver->isReady()) {
			return false;
		}
	}

	_boardAnalogInputs = std::move(inputs);
	_boardPwmOutputs = std::move(outputs);
	_boardAnalogOutputs = std::move(analogOutputs);
	return true;
}

AnalogInput* DeviceManager::analogInput(std::string_view name) {
	for (auto& slot : _boardAnalogInputs) {
		if (slot.driver && slot.name == name) {
			return slot.driver.get();
		}
	}

	return nullptr;
}

AnalogOutput* DeviceManager::analogOutput(std::string_view name) {
	for (auto& slot : _boardAnalogOutputs) {
		if (slot.driver && slot.name == name) {
			return slot.driver.get();
		}
	}

	return nullptr;
}

PwmOutput* DeviceManager::pwmOutput(std::string_view name) {
	for (auto& slot : _boardPwmOutputs) {
		if (slot.driver && slot.name == name) {
			return slot.driver.get();
		}
	}

	return nullptr;
}

} // namespace lexxos::devices
