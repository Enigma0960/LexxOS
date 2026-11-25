#include <new>

#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

namespace {

	using platform::BoardUserGpioPin;
	using platform::GpioDriver;

	DigitalInputConfig inputConfig(BoardUserGpioPin const& pin) {
		auto const timing = pin.inputTiming ? *pin.inputTiming : platform::GpioInputTiming{};
		return {.pin = pin.pin, .pull = pin.defaultPull, .activeLevel = pin.activeLevel, .debounceMs = timing.debounceMs, .clickGapMs = timing.clickGapMs, .holdMs = timing.holdMs, .longHoldMs = timing.longHoldMs};
	}

	bool validBinding(BoardUserGpioPin const& pin, platform::PlatformGpioService const& gpio) {
		if (!pin.isValid() || !*pin.label) {
			return false;
		}

		if (pin.driver == GpioDriver::DigitalOutput) {
			return pin.outputAllowed && gpio.configureOutput && gpio.write;
		}

		auto const config = inputConfig(pin);
		return pin.driver == GpioDriver::DigitalInput && pin.inputAllowed && gpio.configureInput && gpio.read && config.clickGapMs && config.holdMs && config.longHoldMs > config.holdMs;
	}

} // namespace

bool DeviceManager::bindBoardGpio(platform::BoardUserGpio const& board, platform::PlatformGpioService const& gpio) {
	if (board.count > platform::maxBoardUserGpioPins) {
		return false;
	}

	std::size_t count = 0;
	for (std::size_t i = 0; i < board.count; ++i) {
		auto const& pin = board.pins[i];
		if (pin.driver == GpioDriver::None) {
			continue;
		}

		if (!validBinding(pin, gpio)) {
			return false;
		}

		for (std::size_t j = 0; j < board.count; ++j) {
			auto const& other = board.pins[j];
			if (j != i && (other.pin.number == pin.pin.number || (other.label && std::string_view(pin.label) == other.label))) {
				return false;
			}
		}

		++count;
	}

	if (!count) {
		return true;
	}

	if (_gpioPins.count() || count > maxGpioPins) {
		return false;
	}

	std::unique_ptr<GpioDriverInstance[]> drivers{new (std::nothrow) GpioDriverInstance[count]};
	if (!drivers) {
		return false;
	}

	std::size_t next = 0;
	for (std::size_t i = 0; i < board.count; ++i) {
		auto const& pin = board.pins[i];
		if (pin.driver == GpioDriver::None) {
			continue;
		}

		auto& slot = drivers[next++];
		bool ready = pin.driver == GpioDriver::DigitalInput
		    ? slot.emplace<DigitalInput>(inputConfig(pin), gpio).initialize()
		    : slot.emplace<DigitalOutput>(gpio).initialize(pin.pin, pin.initialHigh);
		if (!ready || !_gpioPins.add({pin.label, "board user GPIO", pin.pin, pin.inputAllowed, pin.outputAllowed})) {
			_gpioPins.clear();
			return false;
		}
	}

	_boardDrivers = std::move(drivers);
	_boardDriverCount = count;
	return true;
}

DeviceManager::GpioDriverInstance* DeviceManager::findGpioDriver(std::string_view name) {
	auto const index = _gpioPins.findIndex(name);
	return index < _boardDriverCount ? &_boardDrivers[index] : nullptr;
}

DigitalInput* DeviceManager::digitalInput(std::string_view name) {
	return std::get_if<DigitalInput>(findGpioDriver(name));
}

DigitalOutput* DeviceManager::digitalOutput(std::string_view name) {
	return std::get_if<DigitalOutput>(findGpioDriver(name));
}

bool DeviceManager::pollDigitalInputs(std::uint32_t nowMs) {
	bool ok = true;
	for (std::size_t i = 0; i < _boardDriverCount; ++i) {
		auto* input = std::get_if<DigitalInput>(&_boardDrivers[i]);
		if (input && !input->poll(nowMs)) {
			ok = false;
		}
	}

	return ok;
}

} // namespace lexxos::devices
