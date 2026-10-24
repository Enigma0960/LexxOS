#include <lexxos/devices/input/sega_genesis_gamepad.h>

#include <lexxos/devices/input/sega_genesis_gamepad_types.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t phaseReadAttempts = 3;

} // namespace

SegaGenesisGamepad::SegaGenesisGamepad(
    Pcf8575PortExpander& expander,
    lexxos::platform::SegaGenesisGamepadPins config)
    : SegaGenesisGamepad(expander, config, lexxos::platform::services()) {
}

SegaGenesisGamepad::SegaGenesisGamepad(
    Pcf8575PortExpander& expander,
    lexxos::platform::SegaGenesisGamepadPins config,
    lexxos::platform::PlatformServices const& platform)
    : _expander(expander)
    , _platformServices(platform)
    , _config(config) {
}

bool SegaGenesisGamepad::initialize() {
	if (!_config.isValid() || !_expander.isReady()) {
		_ready = false;
		return false;
	}

	auto const& gpio = _platformServices.gpio;
	_ready = gpio.configureOutput != nullptr && gpio.configureOutput(_config.select, true);
	return _ready;
}

std::string_view SegaGenesisGamepad::name() const {
	return _config.name != nullptr ? std::string_view{_config.name} : std::string_view{};
}

bool SegaGenesisGamepad::isReady() const {
	return _ready && _expander.isReady();
}

GamepadState SegaGenesisGamepad::state() {
	input::detail::SegaGenesisGamepadSample sample{};
	if (!_readSample(sample) || !_hasThreeButtonSignature(sample.lowPhase)) {
		return {};
	}

	return {
	    .up = _isActive(sample.highPhase, _config.up),
	    .down = _isActive(sample.highPhase, _config.down),
	    .left = _isActive(sample.highPhase, _config.left),
	    .right = _isActive(sample.highPhase, _config.right),
	    .a = _isActive(sample.highPhase, _config.buttonCStart),
	    .b = _isActive(sample.highPhase, _config.buttonBa),
	    .start = _isActive(sample.lowPhase, _config.buttonCStart),
	    .select = _isActive(sample.lowPhase, _config.buttonBa),
	};
}

bool SegaGenesisGamepad::isConnected() {
	input::detail::SegaGenesisGamepadSample sample{};
	return _readSample(sample) && _hasThreeButtonSignature(sample.lowPhase);
}

std::uint16_t SegaGenesisGamepad::_pinMask(std::uint8_t pin) const {
	return static_cast<std::uint16_t>(1U << pin);
}

std::uint16_t SegaGenesisGamepad::_inputMask() const {
	return static_cast<std::uint16_t>(_pinMask(_config.up) | _pinMask(_config.down) | _pinMask(_config.left) | _pinMask(_config.right) | _pinMask(_config.buttonBa) | _pinMask(_config.buttonCStart));
}

bool SegaGenesisGamepad::_isActive(std::uint16_t port, std::uint8_t pin) const {
	return (port & _pinMask(pin)) == 0;
}

bool SegaGenesisGamepad::_readPhase(bool selectHigh, std::uint16_t& port) {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write == nullptr || !gpio.write(_config.select, selectHigh)) {
		return false;
	}

	auto const inputMask = _inputMask();
	for (std::uint8_t attempt = 0; attempt < phaseReadAttempts; ++attempt) {
		// Входы PCF8575 должны быть отпущены записью единицы. Повторная
		// запись также восстанавливает latch после прерванной транзакции.
		if (!_expander.releasePins(inputMask)) {
			continue;
		}

		std::uint16_t sample = 0xFFFF;
		if (!_expander.readPort(sample)) {
			continue;
		}

		// Одновременный ноль на всех шести линиях не является допустимым
		// состоянием контроллера и не должен превращаться в нажатие всех кнопок.
		if ((sample & inputMask) == 0) {
			continue;
		}

		port = sample;
		return true;
	}
	return false;
}

bool SegaGenesisGamepad::_readSample(input::detail::SegaGenesisGamepadSample& sample) {
	if (!isReady()) {
		return false;
	}

	auto transaction = _expander.transaction();
	if (!transaction) {
		return false;
	}

	// Чтение обеих фаз сигнала TH.
	auto const highRead = _readPhase(true, sample.highPhase);
	auto const lowRead = highRead && _readPhase(false, sample.lowPhase);

	// После опроса контроллер всегда остаётся в исходной высокой фазе.
	auto const& gpio = _platformServices.gpio;
	auto const restored = gpio.write != nullptr && gpio.write(_config.select, true);
	return highRead && lowRead && restored;
}

bool SegaGenesisGamepad::_hasThreeButtonSignature(std::uint16_t lowPhase) const {
	return _isActive(lowPhase, _config.left) && _isActive(lowPhase, _config.right);
}

} // namespace lexxos::devices
