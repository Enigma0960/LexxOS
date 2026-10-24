#include <lexxos/devices/stm32/parallel_tft_display_stm32.h>

#include <lexxos/devices/stm32/detail/stm32_parallel_tft_support.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

Stm32ParallelTftDisplay::Stm32ParallelTftDisplay(lexxos::platform::ParallelTftDisplayPins config)
    : Stm32ParallelTftDisplay(config, lexxos::platform::services()) {
}

Stm32ParallelTftDisplay::Stm32ParallelTftDisplay(
    lexxos::platform::ParallelTftDisplayPins config,
    lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _config(config) {
}

bool Stm32ParallelTftDisplay::initialize() {
	_ready = false;
	_frameActive = false;

	// Этап 1: проверяем board descriptor и поднимаем 16-битную FSMC-шину.
	if (!_config.isValid() || !stm32::detail::configureParallelTftBus(_config)) {
		return false;
	}
	if (_config.reset.isValid() && !_configureOutput(_config.reset, true)) {
		return false;
	}
	if (_config.backlight.isValid() && !_configureOutput(_config.backlight, false)) {
		return false;
	}

	// Этап 2: держим подсветку выключенной до reset и полной настройки ILI9341.
	_setBacklight(false);
	_hardwareReset();
	stm32::detail::initializeIli9341(_platformServices);

	// Этап 3: очищаем первый кадр и только затем включаем подсветку.
	_ready = true;
	clear(0x0000);
	_setBacklight(true);
	return true;
}

std::string_view Stm32ParallelTftDisplay::name() const {
	return _config.name == nullptr ? "parallel-tft" : _config.name;
}

bool Stm32ParallelTftDisplay::isReady() const {
	return _ready;
}

int Stm32ParallelTftDisplay::width() const {
	return _config.width;
}

int Stm32ParallelTftDisplay::height() const {
	return _config.height;
}

PixelFormat Stm32ParallelTftDisplay::nativeFormat() const {
	return PixelFormat::Rgb565;
}

bool Stm32ParallelTftDisplay::clear(std::uint16_t rgb565) {
	Rect const screen{.x = 0, .y = 0, .width = _config.width, .height = _config.height};
	if (!_ready || !_isRectValid(screen)) {
		return false;
	}

	stm32::detail::setIli9341AddressWindow(screen);
	for (int index = 0; index < _config.width * _config.height; ++index) {
		stm32::detail::writeParallelTftData(rgb565);
	}
	return true;
}

bool Stm32ParallelTftDisplay::beginFrame() {
	if (!_ready) {
		return false;
	}
	_frameActive = true;
	return true;
}

bool Stm32ParallelTftDisplay::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	if (!_ready || !_frameActive || pixels == nullptr || stridePixels < rect.width || !_isRectValid(rect)) {
		return false;
	}

	stm32::detail::setIli9341AddressWindow(rect);
	for (int y = 0; y < rect.height; ++y) {
		auto const* row = pixels + y * stridePixels;
		for (int x = 0; x < rect.width; ++x) {
			stm32::detail::writeParallelTftData(row[x]);
		}
	}
	return true;
}

bool Stm32ParallelTftDisplay::endFrame() {
	if (!_ready || !_frameActive) {
		return false;
	}
	_frameActive = false;
	return true;
}

void Stm32ParallelTftDisplay::_hardwareReset() {
	if (!_config.reset.isValid()) {
		_sleepMs(50);
		return;
	}

	_writeGpio(_config.reset, true);
	_sleepMs(5);
	_writeGpio(_config.reset, false);
	_sleepMs(20);
	_writeGpio(_config.reset, true);
	_sleepMs(150);
}

void Stm32ParallelTftDisplay::_setBacklight(bool enabled) {
	if (_config.backlight.isValid()) {
		_writeGpio(_config.backlight, enabled);
	}
}

bool Stm32ParallelTftDisplay::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= _config.width && rect.y + rect.height <= _config.height;
}

bool Stm32ParallelTftDisplay::_configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const {
	auto const& gpio = _platformServices.gpio;
	return gpio.configureOutput != nullptr && gpio.configureOutput(pin, initialValue);
}

void Stm32ParallelTftDisplay::_writeGpio(lexxos::platform::GpioPin pin, bool value) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(pin, value);
	}
}

void Stm32ParallelTftDisplay::_sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace lexxos::devices
