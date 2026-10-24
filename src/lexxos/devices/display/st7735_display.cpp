#include <lexxos/devices/display/st7735_display.h>

#include <array>

#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/dma.h>
	#include <hardware/gpio.h>
	#include <hardware/spi.h>
	#include <pico/platform.h>
#endif

namespace {

constexpr std::uint8_t commandSoftwareReset = 0x01;
constexpr std::uint8_t commandSleepOut = 0x11;
constexpr std::uint8_t commandSleepIn = 0x10;
constexpr std::uint8_t commandInversionOff = 0x20;
constexpr std::uint8_t commandInversionOn = 0x21;
constexpr std::uint8_t commandDisplayOn = 0x29;
constexpr std::uint8_t commandDisplayOff = 0x28;
constexpr std::uint8_t commandColumnAddressSet = 0x2A;
constexpr std::uint8_t commandRowAddressSet = 0x2B;
constexpr std::uint8_t commandMemoryWrite = 0x2C;
constexpr std::uint8_t commandMemoryDataAccessControl = 0x36;
constexpr std::uint8_t commandInterfacePixelFormat = 0x3A;

constexpr int pixelBurstCount = 512;
#if defined(LEXXOS_PLATFORM_PICO)
constexpr std::size_t dmaWriteThreshold = 256;
#endif

std::uint8_t highByte(std::uint16_t value) {
	return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

std::uint8_t lowByte(std::uint16_t value) {
	return static_cast<std::uint8_t>(value & 0xFFU);
}

#if defined(LEXXOS_PLATFORM_PICO)
spi_inst_t* displaySpiInstance(int index) {
	return index == 1 ? spi1 : spi0;
}

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}
#endif

} // namespace

namespace lexxos::devices {

St7735Display::St7735Display(lexxos::platform::SpiTftDisplayPins config)
    : St7735Display(config, lexxos::platform::services()) {
}

St7735Display::St7735Display(lexxos::platform::SpiTftDisplayPins config, lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _config(config) {
}

bool St7735Display::initialize() {
	_ready = false;
	_frameActive = false;
	_rgb565StreamActive = false;
	_hardwareSpi = false;
	_powerMode = DisplayPowerMode::On;

	if (!_config.isValid() || !_configurePins()) {
		return false;
	}

	_setBacklight(false);
	_hardwareReset();

	_writeCommand(commandSoftwareReset);
	_sleepMs(150);
	_writeCommand(commandSleepOut);
	_sleepMs(120);
	_writeCommand(commandMemoryDataAccessControl);
	_writeData(0x00);
	_writeCommand(commandInterfacePixelFormat);
	_writeData(0x05);
	_writeCommand(_config.invertColors ? commandInversionOn : commandInversionOff);
	_writeCommand(commandDisplayOn);
	_sleepMs(50);

	_ready = true;
	clear(0x0000);
	_setBacklight(true);
	return true;
}

std::string_view St7735Display::name() const {
	return _hardwareSpi ? "st7735-spi" : "st7735";
}

bool St7735Display::isReady() const {
	return _ready;
}

int St7735Display::width() const {
	return _config.width;
}

int St7735Display::height() const {
	return _config.height;
}

PixelFormat St7735Display::nativeFormat() const {
	return PixelFormat::Rgb565;
}

DisplayWireFormat St7735Display::wireFormat() const {
	return DisplayWireFormat::Rgb565;
}

int St7735Display::wireBytesPerPixel() const {
	return 2;
}

std::uint32_t St7735Display::busSpeedHz() const {
	return _hardwareSpi ? _config.spiBaudRate : 0;
}

bool St7735Display::configureBusSpeed(std::uint32_t hz) {
	if (!_ready || _frameActive || _rgb565StreamActive || !_hardwareSpi || hz == 0) {
		return false;
	}
#if defined(LEXXOS_PLATFORM_PICO)
	auto* spi = displaySpiInstance(_config.spiIndex);
	auto const actual = spi_set_baudrate(spi, hz);
	if (actual == 0) {
		return false;
	}
	_config.spiBaudRate = actual;
	return true;
#else
	(void) hz;
	return false;
#endif
}

bool St7735Display::supportsPowerMode() const {
	return true;
}

DisplayPowerMode St7735Display::powerMode() const {
	return _powerMode;
}

bool St7735Display::setPowerMode(DisplayPowerMode mode) {
	if (!_ready || _frameActive || _rgb565StreamActive) {
		return false;
	}

	if (mode == _powerMode) {
		return true;
	}

	switch (mode) {
		case DisplayPowerMode::On:
			if (_powerMode == DisplayPowerMode::Sleep) {
				_writeCommand(commandSleepOut);
				_sleepMs(120);
			}
			_writeCommand(commandDisplayOn);
			_sleepMs(50);
			_setBacklight(true);
			_powerMode = DisplayPowerMode::On;
			return true;
		case DisplayPowerMode::Off:
			_setBacklight(false);
			if (_powerMode == DisplayPowerMode::Sleep) {
				_writeCommand(commandSleepOut);
				_sleepMs(120);
			}
			_writeCommand(commandDisplayOff);
			_powerMode = DisplayPowerMode::Off;
			return true;
		case DisplayPowerMode::Sleep:
			_setBacklight(false);
			_writeCommand(commandDisplayOff);
			_sleepMs(20);
			_writeCommand(commandSleepIn);
			_sleepMs(120);
			_powerMode = DisplayPowerMode::Sleep;
			return true;
	}
	return false;
}

bool St7735Display::clear(std::uint16_t rgb565) {
	if (!_ready || _powerMode != DisplayPowerMode::On) {
		return false;
	}

	if (!_setAddressWindow({.x = 0, .y = 0, .width = _config.width, .height = _config.height})) {
		return false;
	}

	_select();
	_writeGpio(_config.dc, true);
	_writeRepeatedPixel(rgb565, _config.width * _config.height);
	_deselect();
	return true;
}

bool St7735Display::beginFrame() {
	if (!_ready || _powerMode != DisplayPowerMode::On) {
		return false;
	}

	_frameActive = true;
	return true;
}

bool St7735Display::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	if (!_ready || !_frameActive || pixels == nullptr || stridePixels < rect.width || !_isRectValid(rect)) {
		return false;
	}

	if (!beginRgb565Stream(rect)) {
		return false;
	}

	for (int y = 0; y < rect.height; ++y) {
		if (!writeRgb565Stream(pixels + y * stridePixels, rect.width)) {
			endRgb565Stream();
			return false;
		}
	}
	return endRgb565Stream();
}

bool St7735Display::beginRgb565Stream(Rect const& rect) {
	if (!_ready || !_frameActive || _rgb565StreamActive || !_isRectValid(rect)) {
		return false;
	}

	if (!_setAddressWindow(rect)) {
		return false;
	}

	_select();
	_writeGpio(_config.dc, true);
	_rgb565StreamActive = true;
	return true;
}

bool St7735Display::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || pixels == nullptr || count < 0) {
		return false;
	}
	_writePixels(pixels, count);
	return true;
}

bool St7735Display::writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}
	_writeBytes(bytes, static_cast<std::size_t>(pixelCount) * 2U);
	return true;
}

bool St7735Display::endRgb565Stream() {
	if (!_ready || !_frameActive || !_rgb565StreamActive) {
		return false;
	}

	_rgb565StreamActive = false;
	_deselect();
	return true;
}

bool St7735Display::endFrame() {
	if (!_ready || !_frameActive) {
		return false;
	}

	if (_rgb565StreamActive) {
		endRgb565Stream();
	}

	_frameActive = false;
	return true;
}

bool St7735Display::_configurePins() {
	if (!_configureOutput(_config.cs, true) || !_configureOutput(_config.dc, false) || !_configureOutput(_config.reset, true) || (_config.backlight.isValid() && !_configureOutput(_config.backlight, false))) {
		return false;
	}
	if (_config.spiIndex >= 0) {
#if defined(LEXXOS_PLATFORM_PICO)
		auto* spi = displaySpiInstance(_config.spiIndex);
		spi_init(spi, _config.spiBaudRate);
		spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
		gpio_set_function(toPicoGpioPin(_config.sck), GPIO_FUNC_SPI);
		gpio_set_function(toPicoGpioPin(_config.mosi), GPIO_FUNC_SPI);
		gpio_set_drive_strength(toPicoGpioPin(_config.sck), GPIO_DRIVE_STRENGTH_8MA);
		gpio_set_drive_strength(toPicoGpioPin(_config.mosi), GPIO_DRIVE_STRENGTH_8MA);
		_hardwareSpi = true;
		_dmaChannel = dma_claim_unused_channel(false);
		return true;
#else
		return false;
#endif
	}
	return _configureOutput(_config.sck, false) && _configureOutput(_config.mosi, false);
}

void St7735Display::_hardwareReset() {
	_writeGpio(_config.reset, true);
	_sleepMs(5);
	_writeGpio(_config.reset, false);
	_sleepMs(20);
	_writeGpio(_config.reset, true);
	_sleepMs(150);
}

void St7735Display::_select() {
	_writeGpio(_config.cs, false);
}

void St7735Display::_deselect() {
	_writeGpio(_config.cs, true);
}

void St7735Display::_writeByte(std::uint8_t value) {
	_writeBytes(&value, 1);
}

void St7735Display::_writeBytes(std::uint8_t const* data, std::size_t size) {
	if (data == nullptr || size == 0) {
		return;
	}
#if defined(LEXXOS_PLATFORM_PICO)
	if (_hardwareSpi) {
		auto* spi = displaySpiInstance(_config.spiIndex);
		if (_dmaChannel >= 0 && size >= dmaWriteThreshold) {
			auto config = dma_channel_get_default_config(static_cast<uint>(_dmaChannel));
			channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
			channel_config_set_read_increment(&config, true);
			channel_config_set_write_increment(&config, false);
			channel_config_set_dreq(&config, _config.spiIndex == 1 ? DREQ_SPI1_TX : DREQ_SPI0_TX);
			dma_channel_configure(
			    static_cast<uint>(_dmaChannel),
			    &config,
			    &spi_get_hw(spi)->dr,
			    data,
			    static_cast<uint32_t>(size),
			    true);
			dma_channel_wait_for_finish_blocking(static_cast<uint>(_dmaChannel));
			while (spi_is_busy(spi)) {
				tight_loop_contents();
			}
			return;
		}

		spi_write_blocking(spi, data, size);
		return;
	}
#endif
	for (std::size_t index = 0; index < size; ++index) {
		auto const value = data[index];
		for (int bit = 7; bit >= 0; --bit) {
			_writeGpio(_config.sck, false);
			_writeGpio(_config.mosi, ((value >> bit) & 0x01U) != 0);
			_writeGpio(_config.sck, true);
		}
		_writeGpio(_config.sck, false);
	}
}

void St7735Display::_writePixels(std::uint16_t const* pixels, int count) {
	if (pixels == nullptr || count <= 0) {
		return;
	}
	std::array<std::uint8_t, pixelBurstCount * 2> buffer{};
	int offset = 0;
	while (offset < count) {
		auto const chunk = count - offset > pixelBurstCount ? pixelBurstCount : count - offset;
		for (int index = 0; index < chunk; ++index) {
			auto const pixel = pixels[offset + index];
			buffer[static_cast<std::size_t>(index * 2)] = highByte(pixel);
			buffer[static_cast<std::size_t>(index * 2 + 1)] = lowByte(pixel);
		}
		_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * 2));
		offset += chunk;
	}
}

void St7735Display::_writeRepeatedPixel(std::uint16_t rgb565, int count) {
	if (count <= 0) {
		return;
	}
	std::array<std::uint8_t, pixelBurstCount * 2> buffer{};
	for (int index = 0; index < pixelBurstCount; ++index) {
		buffer[static_cast<std::size_t>(index * 2)] = highByte(rgb565);
		buffer[static_cast<std::size_t>(index * 2 + 1)] = lowByte(rgb565);
	}
	while (count > 0) {
		auto const chunk = count > pixelBurstCount ? pixelBurstCount : count;
		_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * 2));
		count -= chunk;
	}
}

void St7735Display::_writeCommand(std::uint8_t command) {
	_select();
	_writeGpio(_config.dc, false);
	_writeByte(command);
	_deselect();
}

void St7735Display::_writeData(std::uint8_t value) {
	_writeData(&value, 1);
}

void St7735Display::_writeData(std::uint8_t const* data, std::size_t size) {
	_select();
	_writeGpio(_config.dc, true);
	_writeBytes(data, size);
	_deselect();
}

bool St7735Display::_setAddressWindow(Rect const& rect) {
	if (!_isRectValid(rect)) {
		return false;
	}

	auto const x0 = static_cast<std::uint16_t>(rect.x + _config.columnOffset);
	auto const y0 = static_cast<std::uint16_t>(rect.y + _config.rowOffset);
	auto const x1 = static_cast<std::uint16_t>(rect.x + rect.width - 1 + _config.columnOffset);
	auto const y1 = static_cast<std::uint16_t>(rect.y + rect.height - 1 + _config.rowOffset);

	std::array<std::uint8_t, 4> columns{highByte(x0), lowByte(x0), highByte(x1), lowByte(x1)};
	std::array<std::uint8_t, 4> rows{highByte(y0), lowByte(y0), highByte(y1), lowByte(y1)};

	_writeCommand(commandColumnAddressSet);
	_writeData(columns.data(), columns.size());
	_writeCommand(commandRowAddressSet);
	_writeData(rows.data(), rows.size());
	_writeCommand(commandMemoryWrite);
	return true;
}

bool St7735Display::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= _config.width && rect.y + rect.height <= _config.height;
}

void St7735Display::_setBacklight(bool enabled) {
	if (_config.backlight.isValid()) {
		_writeGpio(_config.backlight, enabled);
	}
}

bool St7735Display::_configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const {
	auto const& gpio = _platformServices.gpio;
	return gpio.configureOutput != nullptr && gpio.configureOutput(pin, initialValue);
}

void St7735Display::_writeGpio(lexxos::platform::GpioPin pin, bool value) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(pin, value);
	}
}

void St7735Display::_sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace lexxos::devices
