#include <lexxos/devices/espidf/m5stack_lcd_display.h>

#include <array>

#include <lexxos/platform/platform_services.h>

namespace {

constexpr std::uint8_t commandSoftwareReset = 0x01;
constexpr std::uint8_t commandSleepOut = 0x11;
constexpr std::uint8_t commandNormalDisplayModeOn = 0x13;
constexpr std::uint8_t commandInversionOff = 0x20;
constexpr std::uint8_t commandInversionOn = 0x21;
constexpr std::uint8_t commandDisplayOn = 0x29;
constexpr std::uint8_t commandColumnAddressSet = 0x2A;
constexpr std::uint8_t commandRowAddressSet = 0x2B;
constexpr std::uint8_t commandMemoryWrite = 0x2C;
constexpr std::uint8_t commandPorchControl = 0xB2;
constexpr std::uint8_t commandGateControl = 0xB7;
constexpr std::uint8_t commandVcomSetting = 0xBB;
constexpr std::uint8_t commandMemoryDataAccessControl = 0x36;
constexpr std::uint8_t commandInterfacePixelFormat = 0x3A;
constexpr std::uint8_t commandLcmControl = 0xC0;
constexpr std::uint8_t commandVdvVrhEnable = 0xC2;
constexpr std::uint8_t commandVrhSet = 0xC3;
constexpr std::uint8_t commandVdvSet = 0xC4;
constexpr std::uint8_t commandFrameRateControl = 0xC6;
constexpr std::uint8_t commandPowerControl1 = 0xD0;
constexpr std::uint8_t commandPositiveVoltageGammaControl = 0xE0;
constexpr std::uint8_t commandNegativeVoltageGammaControl = 0xE1;

// Стандартная ориентация M5Stack Basic: landscape, кнопки под экраном.
// M5Stack Basic/ILI934x and Cardputer/ST7789 use different MADCTL values and
// address-window transforms. Keep these parameters in the board display
// descriptor instead of hard-coding one controller model in the shared driver.
constexpr std::uint8_t pixelFormatRgb565 = 0x55;
constexpr int pixelBurstCount = 512;

std::uint8_t highByte(std::uint16_t value) {
	return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

std::uint8_t lowByte(std::uint16_t value) {
	return static_cast<std::uint8_t>(value & 0xFFU);
}

bool displaySharesSdBus(lexxos::platform::SpiTftDisplayPins const& display, lexxos::platform::SpiPins const& sd) {
	return sd.isValid() && display.sck.number == sd.sck.number && display.mosi.number == sd.mosi.number && display.spiIndex == sd.index;
}

bool spiBusHasTxPins(lexxos::platform::SpiPins const& pins) {
	return pins.sck.isValid() && pins.mosi.isValid();
}

lexxos::platform::SpiPins displayBusPins(lexxos::platform::SpiTftDisplayPins const& display) {
	return lexxos::platform::SpiPins{.index = display.spiIndex, .sck = display.sck, .mosi = display.mosi, .cs = display.cs, .baudRate = display.spiBaudRate};
}

} // namespace

namespace lexxos::devices {

M5StackLcdDisplay::M5StackLcdDisplay(SpiBus& bus, lexxos::platform::SpiTftDisplayPins config)
    : M5StackLcdDisplay(bus, config, lexxos::platform::services()) {
}

M5StackLcdDisplay::M5StackLcdDisplay(
    SpiBus& bus,
    lexxos::platform::SpiTftDisplayPins config,
    lexxos::platform::PlatformServices const& platform)
    : _bus(bus)
    , _platformServices(platform)
    , _config(config) {
}

bool M5StackLcdDisplay::initialize() {
	_ready = false;
	_frameActive = false;
	_rgb565StreamActive = false;
	_writeActive = false;

	if (!_config.isValid() || !_configurePinsAndBus()) {
		_setBacklight(false);
		return false;
	}

	_setBacklight(false);
	_hardwareReset();

	if (!_writeCommand(commandSoftwareReset)) {
		return false;
	}
	_sleepMs(150);
	if (!_writeCommand(commandSleepOut)) {
		return false;
	}
	_sleepMs(120);
	if (!_initializeController()) {
		return false;
	}
	_sleepMs(50);

	_ready = true;
	if (!clear(0x0000)) {
		_ready = false;
		_setBacklight(false);
		return false;
	}
	_setBacklight(true);
	return true;
}

std::string_view M5StackLcdDisplay::name() const {
	return "m5stack-lcd";
}

bool M5StackLcdDisplay::isReady() const {
	return _ready;
}

int M5StackLcdDisplay::width() const {
	return _config.width;
}

int M5StackLcdDisplay::height() const {
	return _config.height;
}

PixelFormat M5StackLcdDisplay::nativeFormat() const {
	return PixelFormat::Rgb565;
}

bool M5StackLcdDisplay::clear(std::uint16_t rgb565) {
	if (!_ready || !_setAddressWindow({.x = 0, .y = 0, .width = _config.width, .height = _config.height})) {
		return false;
	}

	if (!_beginWrite(true)) {
		return false;
	}
	auto const result = _writeRepeatedPixel(rgb565, _config.width * _config.height);
	_endWrite();
	return result;
}

bool M5StackLcdDisplay::beginFrame() {
	if (!_ready) {
		return false;
	}

	_frameActive = true;
	return true;
}

bool M5StackLcdDisplay::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
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

bool M5StackLcdDisplay::beginRgb565Stream(Rect const& rect) {
	if (!_ready || !_frameActive || _rgb565StreamActive || !_isRectValid(rect)) {
		return false;
	}

	if (!_setAddressWindow(rect) || !_beginWrite(true)) {
		return false;
	}

	_rgb565StreamActive = true;
	return true;
}

bool M5StackLcdDisplay::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || pixels == nullptr || count < 0) {
		return false;
	}
	return _writePixels(pixels, count);
}

bool M5StackLcdDisplay::writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}
	return _writeBytes(bytes, static_cast<std::size_t>(pixelCount) * 2U);
}

bool M5StackLcdDisplay::endRgb565Stream() {
	if (!_ready || !_frameActive || !_rgb565StreamActive) {
		return false;
	}

	_rgb565StreamActive = false;
	_endWrite();
	return true;
}

bool M5StackLcdDisplay::endFrame() {
	if (!_ready || !_frameActive) {
		return false;
	}

	if (_rgb565StreamActive) {
		endRgb565Stream();
	}

	_frameActive = false;
	return true;
}

bool M5StackLcdDisplay::_configurePinsAndBus() {
	if (!_configureOutput(_config.cs, true) || !_configureOutput(_config.dc, false) || !_configureOutput(_config.reset, true) || (_config.backlight.isValid() && !_configureOutput(_config.backlight, false))) {
		return false;
	}

	auto const sdPins = _board().sdSpi;
	auto spiPins = displaySharesSdBus(_config, sdPins) ? sdPins : displayBusPins(_config);
	if (!spiBusHasTxPins(spiPins)) {
		return false;
	}

	if (!_bus.isReady() && !_bus.initialize(spiPins)) {
		return false;
	}

	if (!_bus.setClockMode(SpiClockMode::Mode0)) {
		return false;
	}

	return _prepareBusForLcd();
}

bool M5StackLcdDisplay::_prepareBusForLcd() {
	// LCD and TF card share the same physical SPI bus on M5Stack Basic.
	// The SD backend will temporarily switch the bus to SD-specific speeds,
	// so the LCD restores its own mode and baudrate before every write.
	if (!_bus.setClockMode(SpiClockMode::Mode0)) {
		return false;
	}

	if (_config.spiBaudRate != 0 && !_bus.setBaudRate(_config.spiBaudRate)) {
		return false;
	}

	return true;
}

void M5StackLcdDisplay::_hardwareReset() {
	_writeGpio(_config.reset, true);
	_sleepMs(5);
	_writeGpio(_config.reset, false);
	_sleepMs(20);
	_writeGpio(_config.reset, true);
	_sleepMs(150);
}

bool M5StackLcdDisplay::_initializeController() {
	if (_config.controller == lexxos::platform::SpiTftController::St7789) {
		return _initializeSt7789Controller();
	}
	return _initializeIli934xController();
}

bool M5StackLcdDisplay::_initializeIli934xController() {
	return _writeCommand(commandMemoryDataAccessControl)
	    && _writeData(_config.madctl)
	    && _writeCommand(commandInterfacePixelFormat)
	    && _writeData(pixelFormatRgb565)
	    && _writeCommand(_config.invertColors ? commandInversionOn : commandInversionOff)
	    && _writeCommand(commandDisplayOn);
}

bool M5StackLcdDisplay::_initializeSt7789Controller() {
	constexpr std::uint8_t porchControl[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
	constexpr std::uint8_t powerControl[] = {0xA4, 0xA1};
	constexpr std::uint8_t positiveGamma[] = {
	    0xD0,
	    0x04,
	    0x0D,
	    0x11,
	    0x13,
	    0x2B,
	    0x3F,
	    0x54,
	    0x4C,
	    0x18,
	    0x0D,
	    0x0B,
	    0x1F,
	    0x23,
	};
	constexpr std::uint8_t negativeGamma[] = {
	    0xD0,
	    0x04,
	    0x0C,
	    0x11,
	    0x13,
	    0x2C,
	    0x3F,
	    0x44,
	    0x51,
	    0x2F,
	    0x1F,
	    0x1F,
	    0x20,
	    0x23,
	};

	return _writeCommand(commandNormalDisplayModeOn)
	    && _writeCommand(commandMemoryDataAccessControl)
	    && _writeData(_config.madctl)
	    && _writeCommand(commandInterfacePixelFormat)
	    && _writeData(pixelFormatRgb565)
	    && _writeCommand(commandPorchControl)
	    && _writeData(porchControl, sizeof(porchControl))
	    && _writeCommand(commandGateControl)
	    && _writeData(0x35)
	    && _writeCommand(commandVcomSetting)
	    && _writeData(0x19)
	    && _writeCommand(commandLcmControl)
	    && _writeData(0x2C)
	    && _writeCommand(commandVdvVrhEnable)
	    && _writeData(0x01)
	    && _writeCommand(commandVrhSet)
	    && _writeData(0x12)
	    && _writeCommand(commandVdvSet)
	    && _writeData(0x20)
	    && _writeCommand(commandFrameRateControl)
	    && _writeData(0x0F)
	    && _writeCommand(commandPowerControl1)
	    && _writeData(powerControl, sizeof(powerControl))
	    && _writeCommand(commandPositiveVoltageGammaControl)
	    && _writeData(positiveGamma, sizeof(positiveGamma))
	    && _writeCommand(commandNegativeVoltageGammaControl)
	    && _writeData(negativeGamma, sizeof(negativeGamma))
	    && _writeCommand(_config.invertColors ? commandInversionOn : commandInversionOff)
	    && _writeCommand(commandDisplayOn);
}

bool M5StackLcdDisplay::_writeCommand(std::uint8_t command) {
	if (!_beginWrite(false)) {
		return false;
	}
	auto const result = _writeBytes(&command, 1);
	_endWrite();
	return result;
}

bool M5StackLcdDisplay::_writeData(std::uint8_t value) {
	return _writeData(&value, 1);
}

bool M5StackLcdDisplay::_writeData(std::uint8_t const* data, std::size_t size) {
	if (!_beginWrite(true)) {
		return false;
	}
	auto const result = _writeBytes(data, size);
	_endWrite();
	return result;
}

bool M5StackLcdDisplay::_setAddressWindow(Rect const& rect) {
	if (!_isRectValid(rect)) {
		return false;
	}

	if (_config.addressWindowSwapsAxes) {
		auto const column0 = static_cast<std::uint16_t>(rect.y + _config.rowOffset);
		auto const column1 = static_cast<std::uint16_t>(rect.y + rect.height - 1 + _config.rowOffset);
		auto const page0 = static_cast<std::uint16_t>(rect.x + _config.columnOffset);
		auto const page1 = static_cast<std::uint16_t>(rect.x + rect.width - 1 + _config.columnOffset);

		std::uint8_t columns[] = {highByte(column0), lowByte(column0), highByte(column1), lowByte(column1)};
		std::uint8_t pages[] = {highByte(page0), lowByte(page0), highByte(page1), lowByte(page1)};

		return _writeCommand(commandColumnAddressSet)
		    && _writeData(columns, sizeof(columns))
		    && _writeCommand(commandRowAddressSet)
		    && _writeData(pages, sizeof(pages))
		    && _writeCommand(commandMemoryWrite);
	}

	auto const column0 = static_cast<std::uint16_t>(rect.x + _config.columnOffset);
	auto const column1 = static_cast<std::uint16_t>(rect.x + rect.width - 1 + _config.columnOffset);
	auto const page0 = static_cast<std::uint16_t>(rect.y + _config.rowOffset);
	auto const page1 = static_cast<std::uint16_t>(rect.y + rect.height - 1 + _config.rowOffset);

	std::uint8_t columns[] = {highByte(column0), lowByte(column0), highByte(column1), lowByte(column1)};
	std::uint8_t rows[] = {highByte(page0), lowByte(page0), highByte(page1), lowByte(page1)};

	return _writeCommand(commandColumnAddressSet) && _writeData(columns, sizeof(columns)) && _writeCommand(commandRowAddressSet) && _writeData(rows, sizeof(rows)) && _writeCommand(commandMemoryWrite);
}

bool M5StackLcdDisplay::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= _config.width && rect.y + rect.height <= _config.height;
}

void M5StackLcdDisplay::_setBacklight(bool enabled) {
	if (_config.backlight.isValid()) {
		_writeGpio(_config.backlight, enabled);
	}
}

bool M5StackLcdDisplay::_beginWrite(bool dataMode) {
	if (_writeActive || !_prepareBusForLcd() || !_bus.beginTransaction()) {
		return false;
	}
	_writeActive = true;
	_writeGpio(_config.cs, false);
	_writeGpio(_config.dc, dataMode);
	return true;
}

void M5StackLcdDisplay::_endWrite() {
	if (!_writeActive) {
		return;
	}
	_writeGpio(_config.cs, true);
	_bus.endTransaction();
	_writeActive = false;
}

bool M5StackLcdDisplay::_writeBytes(std::uint8_t const* data, std::size_t size) {
	if (data == nullptr || size == 0) {
		return size == 0;
	}
	return _bus.write(data, size);
}

bool M5StackLcdDisplay::_writePixels(std::uint16_t const* pixels, int count) {
	if (pixels == nullptr || count <= 0) {
		return count == 0;
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
		if (!_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * 2))) {
			return false;
		}
		offset += chunk;
	}
	return true;
}

bool M5StackLcdDisplay::_writeRepeatedPixel(std::uint16_t rgb565, int count) {
	if (count <= 0) {
		return count == 0;
	}

	std::array<std::uint8_t, pixelBurstCount * 2> buffer{};
	for (int index = 0; index < pixelBurstCount; ++index) {
		buffer[static_cast<std::size_t>(index * 2)] = highByte(rgb565);
		buffer[static_cast<std::size_t>(index * 2 + 1)] = lowByte(rgb565);
	}

	while (count > 0) {
		auto const chunk = count > pixelBurstCount ? pixelBurstCount : count;
		if (!_writeBytes(buffer.data(), static_cast<std::size_t>(chunk * 2))) {
			return false;
		}
		count -= chunk;
	}
	return true;
}

lexxos::platform::BoardDescriptor const& M5StackLcdDisplay::_board() const {
	auto const& board = _platformServices.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

bool M5StackLcdDisplay::_configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const {
	auto const& gpio = _platformServices.gpio;
	return gpio.configureOutput != nullptr && gpio.configureOutput(pin, initialValue);
}

void M5StackLcdDisplay::_writeGpio(lexxos::platform::GpioPin pin, bool value) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(pin, value);
	}
}

void M5StackLcdDisplay::_sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace lexxos::devices
