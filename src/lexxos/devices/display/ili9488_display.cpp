#include <lexxos/devices/display/ili9488_display.h>

#include <array>
#include <cstddef>

#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/dma.h>
	#include <hardware/gpio.h>
	#include <hardware/spi.h>
	#include <pico/platform.h>
#endif

namespace {

constexpr std::uint8_t commandSleepOut = 0x11;
constexpr std::uint8_t commandSleepIn = 0x10;
constexpr std::uint8_t commandInversionOff = 0x20;
constexpr std::uint8_t commandInversionOn = 0x21;
constexpr std::uint8_t commandDisplayOn = 0x29;
constexpr std::uint8_t commandDisplayOff = 0x28;
constexpr std::uint8_t commandColumnAddressSet = 0x2A;
constexpr std::uint8_t commandPageAddressSet = 0x2B;
constexpr std::uint8_t commandMemoryWrite = 0x2C;
constexpr std::uint8_t commandMemoryAccessControl = 0x36;
constexpr std::uint8_t commandPixelFormat = 0x3A;
constexpr std::uint8_t commandVerticalScrollingDefinition = 0x33;
constexpr std::uint8_t commandVerticalScrollingStartAddress = 0x37;

constexpr int repeatedPixelBurstCount = 1024;
constexpr int ili9488NativeHeightPixels = 480;
constexpr std::size_t dmaWriteThresholdBytes = 96;

#if !defined(LEXXOS_ILI9488_RGB888_BURST_PIXELS)
	#if defined(PICO_RP2040) && PICO_RP2040
constexpr int rgb888BurstPixels = 512;
	#else
constexpr int rgb888BurstPixels = 4096;
	#endif
#else
constexpr int rgb888BurstPixels = LEXXOS_ILI9488_RGB888_BURST_PIXELS;
#endif
static_assert(rgb888BurstPixels > 0, "LEXXOS_ILI9488_RGB888_BURST_PIXELS must be positive");

std::uint8_t highByte(std::uint16_t value) {
	return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

std::uint8_t lowByte(std::uint16_t value) {
	return static_cast<std::uint8_t>(value & 0xFFU);
}

#if defined(LEXXOS_ILI9488_RGB565_WIRE) && LEXXOS_ILI9488_RGB565_WIRE
constexpr bool ili9488Rgb565WireFormat = true;
constexpr std::uint8_t ili9488PixelFormatValue = 0x55;
constexpr int ili9488WireBytesPerPixel = 2;
#else
constexpr bool ili9488Rgb565WireFormat = false;
constexpr std::uint8_t ili9488PixelFormatValue = 0x66;
constexpr int ili9488WireBytesPerPixel = 3;
#endif
std::uint16_t rgb888ToRgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
	return static_cast<std::uint16_t>(
	    ((static_cast<std::uint16_t>(red) & 0xF8U) << 8U)
	    | ((static_cast<std::uint16_t>(green) & 0xFCU) << 3U)
	    | (static_cast<std::uint16_t>(blue) >> 3U));
}

std::uint8_t rgb565Red8(std::uint16_t value) {
	auto const red = static_cast<std::uint8_t>((value >> 11U) & 0x1FU);
	return static_cast<std::uint8_t>((red << 3U) | (red >> 2U));
}

std::uint8_t rgb565Green8(std::uint16_t value) {
	auto const green = static_cast<std::uint8_t>((value >> 5U) & 0x3FU);
	return static_cast<std::uint8_t>((green << 2U) | (green >> 4U));
}

std::uint8_t rgb565Blue8(std::uint16_t value) {
	auto const blue = static_cast<std::uint8_t>(value & 0x1FU);
	return static_cast<std::uint8_t>((blue << 3U) | (blue >> 2U));
}

void appendRgb565AsRgb888(std::uint8_t*& output, std::uint16_t value) {
	*output++ = rgb565Red8(value);
	*output++ = rgb565Green8(value);
	*output++ = rgb565Blue8(value);
}

void appendRgb565AsRgb565(std::uint8_t*& output, std::uint16_t value) {
	*output++ = highByte(value);
	*output++ = lowByte(value);
}

void fillRgb888FromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, int count) {
	auto* out = output;
	for (int index = 0; index < count; ++index) {
		appendRgb565AsRgb888(out, pixels[index]);
	}
}

void fillRgb888FromRgb565Bytes(std::uint8_t* output, std::uint8_t const* bytes, int pixelCount) {
	auto* out = output;
	for (int index = 0; index < pixelCount; ++index) {
		auto const base = static_cast<std::size_t>(index * 2);
		auto const pixel = static_cast<std::uint16_t>(
		    (static_cast<std::uint16_t>(bytes[base]) << 8U) | bytes[base + 1U]);
		appendRgb565AsRgb888(out, pixel);
	}
}

void fillRgb565WireFromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, int count) {
	auto* out = output;
	for (int index = 0; index < count; ++index) {
		appendRgb565AsRgb565(out, pixels[index]);
	}
}

void fillRgb565WireFromRgb888Bytes(std::uint8_t* output, std::uint8_t const* bytes, int pixelCount) {
	auto* out = output;
	for (int index = 0; index < pixelCount; ++index) {
		auto const base = static_cast<std::size_t>(index * 3);
		appendRgb565AsRgb565(out, rgb888ToRgb565(bytes[base], bytes[base + 1U], bytes[base + 2U]));
	}
}

void fillRepeatedRgb565WirePixel(std::uint8_t* output, std::uint16_t rgb565, int count) {
	auto* out = output;
	for (int index = 0; index < count; ++index) {
		appendRgb565AsRgb565(out, rgb565);
	}
}

int nativeScrollHeightPixels(int configuredHeight) {
	return configuredHeight > ili9488NativeHeightPixels ? configuredHeight : ili9488NativeHeightPixels;
}

std::array<std::uint8_t, repeatedPixelBurstCount * 3> g_repeatedPixelWriteBuffer{};
std::array<std::uint8_t, rgb888BurstPixels * 3> g_pixelWriteBuffer{};

#if defined(LEXXOS_PLATFORM_PICO)
spi_inst_t* displaySpiInstance(int index) {
	return index == 1 ? spi1 : spi0;
}

void waitForDisplaySpiIdle(spi_inst_t* spi) {
	if (spi == nullptr) {
		return;
	}
	while (spi_is_busy(spi)) {
		tight_loop_contents();
	}
}

bool writeDisplaySpiDma(spi_inst_t* spi, int channel, std::uint8_t const* data, std::size_t size) {
	if (spi == nullptr || channel < 0 || data == nullptr || size == 0) {
		return false;
	}
	auto config = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
	channel_config_set_read_increment(&config, true);
	channel_config_set_write_increment(&config, false);
	channel_config_set_dreq(&config, spi_get_dreq(spi, true));
	dma_channel_configure(channel, &config, &spi_get_hw(spi)->dr, data, static_cast<std::uint32_t>(size), true);
	dma_channel_wait_for_finish_blocking(channel);
	waitForDisplaySpiIdle(spi);
	return true;
}

bool writeDisplaySpiDmaRepeatedByte(spi_inst_t* spi, int channel, std::uint8_t const* value, std::size_t size) {
	if (spi == nullptr || channel < 0 || value == nullptr || size == 0) {
		return false;
	}
	auto config = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
	channel_config_set_read_increment(&config, false);
	channel_config_set_write_increment(&config, false);
	channel_config_set_dreq(&config, spi_get_dreq(spi, true));
	dma_channel_configure(channel, &config, &spi_get_hw(spi)->dr, value, static_cast<std::uint32_t>(size), true);
	dma_channel_wait_for_finish_blocking(channel);
	waitForDisplaySpiIdle(spi);
	return true;
}

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}
#endif

bool rgb565ExpandsToRepeatedRgb888Byte(std::uint16_t rgb565, std::uint8_t& value) {
	auto const red = rgb565Red8(rgb565);
	auto const green = rgb565Green8(rgb565);
	auto const blue = rgb565Blue8(rgb565);
	if (red != green || red != blue) {
		return false;
	}
	value = red;
	return true;
}

} // namespace

namespace lexxos::devices {

Ili9488Display::Ili9488Display(lexxos::platform::SpiTftDisplayPins config)
    : Ili9488Display(config, lexxos::platform::services()) {
}

Ili9488Display::Ili9488Display(lexxos::platform::SpiTftDisplayPins config, lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _config(config) {
}

bool Ili9488Display::initialize() {
	_ready = false;
	_frameActive = false;
	_verticalScrollAreaPixels = 0;
	_verticalScrollTopFixedPixels = 0;
	_rgb565StreamActive = false;
	_hardwareSpi = false;
	_powerMode = DisplayPowerMode::On;

#if defined(LEXXOS_PLATFORM_PICO)
	_dmaChannel = -1;
#endif

	if (!_config.isValid() || !_configurePins()) {
		return false;
	}

	_hardwareReset();
	_initializeController();

	_ready = true;
	clear(0x0000);
	return true;
}

std::string_view Ili9488Display::name() const {
	return _hardwareSpi ? "ili9488-spi" : "ili9488";
}

bool Ili9488Display::isReady() const {
	return _ready;
}

int Ili9488Display::width() const {
	return _config.width;
}

int Ili9488Display::height() const {
	return _config.height;
}

PixelFormat Ili9488Display::nativeFormat() const {
	return PixelFormat::Rgb565;
}

DisplayWireFormat Ili9488Display::wireFormat() const {
	return ili9488Rgb565WireFormat ? DisplayWireFormat::Rgb565 : DisplayWireFormat::Rgb666;
}

int Ili9488Display::wireBytesPerPixel() const {
	return ili9488WireBytesPerPixel;
}

std::uint32_t Ili9488Display::busSpeedHz() const {
	return _hardwareSpi ? _config.spiBaudRate : 0;
}

bool Ili9488Display::configureBusSpeed(std::uint32_t hz) {
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

bool Ili9488Display::supportsPowerMode() const {
	return true;
}

DisplayPowerMode Ili9488Display::powerMode() const {
	return _powerMode;
}

bool Ili9488Display::setPowerMode(DisplayPowerMode mode) {
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
			_sleepMs(20);
			_powerMode = DisplayPowerMode::On;
			return true;
		case DisplayPowerMode::Off:
			if (_powerMode == DisplayPowerMode::Sleep) {
				_writeCommand(commandSleepOut);
				_sleepMs(120);
			}
			_writeCommand(commandDisplayOff);
			_powerMode = DisplayPowerMode::Off;
			return true;
		case DisplayPowerMode::Sleep:
			_writeCommand(commandDisplayOff);
			_sleepMs(20);
			_writeCommand(commandSleepIn);
			_sleepMs(120);
			_powerMode = DisplayPowerMode::Sleep;
			return true;
	}
	return false;
}

bool Ili9488Display::clear(std::uint16_t rgb565) {
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

bool Ili9488Display::beginFrame() {
	if (!_ready || _powerMode != DisplayPowerMode::On) {
		return false;
	}

	_frameActive = true;
	return true;
}

bool Ili9488Display::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
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

bool Ili9488Display::fillRgb565(Rect const& rect, std::uint16_t rgb565) {
	if (!_ready || !_frameActive || !_isRectValid(rect)) {
		return false;
	}

	if (!beginRgb565Stream(rect)) {
		return false;
	}

	_writeRepeatedPixel(rgb565, rect.width * rect.height);
	return endRgb565Stream();
}

bool Ili9488Display::beginRgb565Stream(Rect const& rect) {
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

bool Ili9488Display::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || pixels == nullptr || count < 0) {
		return false;
	}
	_writePixels(pixels, count);
	return true;
}

bool Ili9488Display::writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}

	if constexpr (ili9488Rgb565WireFormat) {
		_writeBytes(bytes, static_cast<std::size_t>(pixelCount * ili9488WireBytesPerPixel));
		return true;
	}

	auto& buffer = g_pixelWriteBuffer;
	int offset = 0;
	while (offset < pixelCount) {
		auto const remaining = pixelCount - offset;
		auto const chunk = remaining > rgb888BurstPixels ? rgb888BurstPixels : remaining;
		fillRgb888FromRgb565Bytes(buffer.data(), bytes + static_cast<std::size_t>(offset * 2), chunk);
		_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * ili9488WireBytesPerPixel));
		offset += chunk;
	}
	return true;
}

bool Ili9488Display::writeRgb888Stream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}

	if constexpr (ili9488Rgb565WireFormat) {
		auto& buffer = g_pixelWriteBuffer;
		int offset = 0;
		while (offset < pixelCount) {
			auto const remaining = pixelCount - offset;
			auto const chunk = remaining > rgb888BurstPixels ? rgb888BurstPixels : remaining;
			fillRgb565WireFromRgb888Bytes(buffer.data(), bytes + static_cast<std::size_t>(offset * 3), chunk);
			_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * ili9488WireBytesPerPixel));
			offset += chunk;
		}
		return true;
	}

	_writeBytes(bytes, static_cast<std::size_t>(pixelCount) * 3U);
	return true;
}

bool Ili9488Display::endRgb565Stream() {
	if (!_ready || !_frameActive || !_rgb565StreamActive) {
		return false;
	}

	_rgb565StreamActive = false;
	_deselect();
	return true;
}

bool Ili9488Display::configureVerticalScroll(int topFixedPixels, int scrollAreaPixels, int bottomFixedPixels) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _rgb565StreamActive || topFixedPixels < 0 || scrollAreaPixels <= 0 || bottomFixedPixels < 0) {
		return false;
	}

	auto const requestedHeightPixels = topFixedPixels + scrollAreaPixels + bottomFixedPixels;
	auto const scrollHeightPixels = nativeScrollHeightPixels(_config.height);
	if (requestedHeightPixels != _config.height && requestedHeightPixels != scrollHeightPixels) {
		return false;
	}

	if (requestedHeightPixels > scrollHeightPixels) {
		return false;
	}

	if (requestedHeightPixels < scrollHeightPixels) {
		bottomFixedPixels += scrollHeightPixels - requestedHeightPixels;
	}

	std::array<std::uint8_t, 6> data{
	    highByte(static_cast<std::uint16_t>(topFixedPixels)),
	    lowByte(static_cast<std::uint16_t>(topFixedPixels)),
	    highByte(static_cast<std::uint16_t>(scrollAreaPixels)),
	    lowByte(static_cast<std::uint16_t>(scrollAreaPixels)),
	    highByte(static_cast<std::uint16_t>(bottomFixedPixels)),
	    lowByte(static_cast<std::uint16_t>(bottomFixedPixels)),
	};

	_writeCommand(commandVerticalScrollingDefinition);
	_writeData(data.data(), data.size());
	_verticalScrollAreaPixels = scrollAreaPixels;
	_verticalScrollTopFixedPixels = topFixedPixels;
	return true;
}

bool Ili9488Display::setVerticalScrollStart(int startPixel) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _rgb565StreamActive || _verticalScrollAreaPixels <= 0) {
		return false;
	}

	auto normalized = startPixel % _verticalScrollAreaPixels;
	if (normalized < 0) {
		normalized += _verticalScrollAreaPixels;
	}
	auto const address = _verticalScrollTopFixedPixels + normalized;
	std::array<std::uint8_t, 2> data{highByte(static_cast<std::uint16_t>(address)), lowByte(static_cast<std::uint16_t>(address))};
	_writeCommand(commandVerticalScrollingStartAddress);
	_writeData(data.data(), data.size());
	return true;
}

bool Ili9488Display::endFrame() {
	if (!_ready || !_frameActive) {
		return false;
	}

	if (_rgb565StreamActive) {
		endRgb565Stream();
	}

	_frameActive = false;
	return true;
}

bool Ili9488Display::_configurePins() {
	if (!_configureOutput(_config.cs, true) || !_configureOutput(_config.dc, false) || !_configureOutput(_config.reset, true)) {
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

void Ili9488Display::_hardwareReset() {
	_writeGpio(_config.reset, true);
	_sleepMs(10);
	_writeGpio(_config.reset, false);
	_sleepMs(10);
	_writeGpio(_config.reset, true);
	_sleepMs(200);
}

void Ili9488Display::_initializeController() {
	constexpr std::uint8_t positiveGamma[]{0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F};
	constexpr std::uint8_t negativeGamma[]{0x00, 0x16, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F};
	constexpr std::uint8_t powerControl1[]{0x17, 0x15};
	constexpr std::uint8_t vcomControl[]{0x00, 0x12, 0x80};
	constexpr std::uint8_t displayFunctionControl[]{0x02, 0x02, 0x3B};
	constexpr std::uint8_t adjustControl3[]{0xA9, 0x51, 0x2C, 0x82};

	_writeCommand(0xE0);
	_writeData(positiveGamma, sizeof(positiveGamma));
	_writeCommand(0xE1);
	_writeData(negativeGamma, sizeof(negativeGamma));
	_writeCommand(0xC0);
	_writeData(powerControl1, sizeof(powerControl1));
	_writeCommand(0xC1);
	_writeData(0x41);
	_writeCommand(0xC5);
	_writeData(vcomControl, sizeof(vcomControl));
	_writeCommand(commandMemoryAccessControl);
	_writeData(0x48);
	_writeCommand(commandPixelFormat);
	_writeData(ili9488PixelFormatValue);
	_writeCommand(0xB0);
	_writeData(0x00);
	_writeCommand(0xB1);
	_writeData(0xA0);
	_writeCommand(_config.invertColors ? commandInversionOn : commandInversionOff);
	_writeCommand(0xB4);
	_writeData(0x02);
	_writeCommand(0xB6);
	_writeData(displayFunctionControl, sizeof(displayFunctionControl));
	_writeCommand(0xB7);
	_writeData(0xC6);
	_writeCommand(0xE9);
	_writeData(0x00);
	_writeCommand(0xF7);
	_writeData(adjustControl3, sizeof(adjustControl3));
	_writeCommand(commandSleepOut);
	_sleepMs(120);
	_writeCommand(commandDisplayOn);
	_sleepMs(120);
	_writeCommand(commandMemoryAccessControl);
	_writeData(0x48);
}

void Ili9488Display::_select() {
	_writeGpio(_config.cs, false);
}

void Ili9488Display::_deselect() {
	_writeGpio(_config.cs, true);
}

void Ili9488Display::_writeBytes(std::uint8_t const* data, std::size_t size) {
	if (data == nullptr || size == 0) {
		return;
	}
#if defined(LEXXOS_PLATFORM_PICO)
	if (_hardwareSpi) {
		// spi_write_blocking(displaySpiInstance(_config.spiIndex), data, size);

		auto* spi = displaySpiInstance(_config.spiIndex);
		if (_dmaChannel >= 0 && size >= dmaWriteThresholdBytes && writeDisplaySpiDma(spi, _dmaChannel, data, size)) {
			return;
		}
		spi_write_blocking(spi, data, size);
		waitForDisplaySpiIdle(spi);
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

void Ili9488Display::_writeRepeatedByte(std::uint8_t value, std::size_t size) {
	if (size == 0) {
		return;
	}

	// Do not use DMA with read_increment=false from a single stack byte here.
	// RP2350 can hardfault on long SPI-DMA transfers from a constant byte source.
	auto& buffer = g_repeatedPixelWriteBuffer;
	for (auto& byte : buffer) {
		byte = value;
	}
	while (size > 0) {
		auto const chunk = size > buffer.size() ? buffer.size() : size;
		_writeBytes(buffer.data(), chunk);
		size -= chunk;
	}
}

void Ili9488Display::_writePixels(std::uint16_t const* pixels, int count) {
	if (pixels == nullptr || count <= 0) {
		return;
	}

	auto& buffer = g_pixelWriteBuffer;
	int offset = 0;
	while (offset < count) {
		auto const remaining = count - offset;
		auto const chunk = remaining > rgb888BurstPixels ? rgb888BurstPixels : remaining;
		if constexpr (ili9488Rgb565WireFormat) {
			fillRgb565WireFromRgb565Pixels(buffer.data(), pixels + offset, chunk);
			_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * ili9488WireBytesPerPixel));
		} else {
			fillRgb888FromRgb565Pixels(buffer.data(), pixels + offset, chunk);
			_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * ili9488WireBytesPerPixel));
		}
		offset += chunk;
	}
}

void Ili9488Display::_writeRepeatedPixel(std::uint16_t rgb565, int count) {
	if (count <= 0) {
		return;
	}

	std::uint8_t repeatedByte = 0;
	if constexpr (!ili9488Rgb565WireFormat) {
		if (rgb565ExpandsToRepeatedRgb888Byte(rgb565, repeatedByte)) {
			_writeRepeatedByte(repeatedByte, static_cast<std::size_t>(count) * 3U);
			return;
		}
	}

	auto& buffer = g_repeatedPixelWriteBuffer;
	if constexpr (ili9488Rgb565WireFormat) {
		fillRepeatedRgb565WirePixel(buffer.data(), rgb565, repeatedPixelBurstCount);
	} else {
		for (int index = 0; index < repeatedPixelBurstCount; ++index) {
			auto const base = static_cast<std::size_t>(index * 3);
			buffer[base] = rgb565Red8(rgb565);
			buffer[base + 1U] = rgb565Green8(rgb565);
			buffer[base + 2U] = rgb565Blue8(rgb565);
		}
	}

	while (count > 0) {
		auto const chunk = count > repeatedPixelBurstCount ? repeatedPixelBurstCount : count;
		_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * ili9488WireBytesPerPixel));
		count -= chunk;
	}
}

void Ili9488Display::_writeCommand(std::uint8_t command) {
	_select();
	_writeGpio(_config.dc, false);
	_writeBytes(&command, 1);
	_deselect();
}

void Ili9488Display::_writeData(std::uint8_t value) {
	_writeData(&value, 1);
}

void Ili9488Display::_writeData(std::uint8_t const* data, std::size_t size) {
	_select();
	_writeGpio(_config.dc, true);
	_writeBytes(data, size);
	_deselect();
}

bool Ili9488Display::_setAddressWindow(Rect const& rect) {
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
	_writeCommand(commandPageAddressSet);
	_writeData(rows.data(), rows.size());
	_writeCommand(commandMemoryWrite);
	return true;
}

bool Ili9488Display::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= _config.width && rect.y + rect.height <= _config.height;
}

bool Ili9488Display::_configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const {
	auto const& gpio = _platformServices.gpio;
	return gpio.configureOutput != nullptr && gpio.configureOutput(pin, initialValue);
}

void Ili9488Display::_writeGpio(lexxos::platform::GpioPin pin, bool value) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(pin, value);
	}
}

void Ili9488Display::_sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace lexxos::devices
