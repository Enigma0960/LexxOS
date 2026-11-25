#include <lexxos/devices/display/ili9488_display.h>

#include <array>
#include <cstddef>
#include <cstring>

#include <lexxos/drivers/core/display/ili9488_commands.h>
#include <lexxos/drivers/core/display/ili9488_init.h>
#include <lexxos/drivers/core/display/ili9488_pixels.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform_services.h>

namespace {

constexpr int ili9488NativeHeightPixels = 480;

int nativeScrollHeightPixels(int configuredHeight) {
	return configuredHeight > ili9488NativeHeightPixels ? configuredHeight : ili9488NativeHeightPixels;
}

} // namespace

namespace lexxos::devices {

Ili9488Display::Ili9488Display(Ili9488Panel config,
    Ili9488Backend& backend, Ili9488Buffers buffers, BusTransactionQueue* queue)
    : _config(config)
    , _backend(backend)
    , _buffers(buffers)
    , _busQueue(queue) {
}

Ili9488Display::~Ili9488Display() {
	if (_rgb565StreamActive) {
		_deselect();
	}
}

bool Ili9488Display::_acquireBus(std::optional<BusTransaction>& transaction) {
	if (_busQueue == nullptr || _frameActive) {
		return true;
	}
	transaction.emplace(*_busQueue);
	return bool(*transaction);
}

bool Ili9488Display::initialize() {
	if (_config.wireFormat != drivers::core::Ili9488PixelFormat::Rgb565 && _config.wireFormat != drivers::core::Ili9488PixelFormat::Rgb666) {
		return false;
	}

	if (!_buffers.valid() || _config.width <= 0 || _config.height <= 0 || _config.columnOffset < 0 || _config.rowOffset < 0 || static_cast<std::int64_t>(_config.columnOffset) + _config.width > 65536 || static_cast<std::int64_t>(_config.rowOffset) + _config.height > 65536) {
		return false;
	}

	if (_frameActive || _rgb565StreamActive) {
		return false;
	}

	std::optional<BusTransaction> transaction;
	if (!_acquireBus(transaction)) {
		return false;
	}

	_ioFailed = false;
#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] initialize enter\n");
#endif

	_ready = false;
	_frameActive = false;
	_verticalScrollAreaPixels = 0;
	_verticalScrollTopFixedPixels = 0;
	_rgb565StreamActive = false;

	_powerMode = DisplayPowerMode::On;

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] configure pins begin\n");
#endif

	if (!_backend.initialize()) {
#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
		lexxos::platform::writeConsole("[ili9488] configure pins FAILED\n");
#endif
		return false;
	}

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] hardware reset begin\n");
#endif

	_hardwareReset();

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] controller init begin\n");
#endif

	_initializeController();

	_ready = !_ioFailed;

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] clear screen begin\n");
#endif

	transaction.reset();
	if (!_ready || !clear(0x0000)) {
		return false;
	}

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole(_ready ? "[ili9488] clear OK\n" : "[ili9488] clear FAILED\n");
#endif

	return _ready;
}

std::string_view Ili9488Display::name() const {
	return _backend.hardwareSpi() ? "ili9488-spi" : "ili9488";
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
	return _config.wireFormat == drivers::core::Ili9488PixelFormat::Rgb565 ? DisplayWireFormat::Rgb565 : DisplayWireFormat::Rgb666;
}

int Ili9488Display::wireBytesPerPixel() const {
	return _config.wireFormat == drivers::core::Ili9488PixelFormat::Rgb565 ? 2 : 3;
}

std::uint32_t Ili9488Display::busSpeedHz() const {
	return _backend.busSpeedHz();
}

bool Ili9488Display::configureBusSpeed(std::uint32_t hz) {
	if (!_ready || _frameActive || _rgb565StreamActive || !_backend.hardwareSpi() || !hz) {
		return false;
	}
	std::optional<BusTransaction> transaction;
	return _acquireBus(transaction) && _backend.configureBusSpeed(hz);
}

void Ili9488Display::_writeBytes(std::uint8_t const* data, std::size_t size) {
	if (_ioFailed) {
		return;
	}

	if (!_backend.write(data, size)) {
		_ioFailed = true;
		_ready = false;
	}
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

	std::optional<BusTransaction> transaction;
	if (!_acquireBus(transaction)) {
		return false;
	}

	if (mode == _powerMode) {
		return _ready;
	}

	auto convert = [](DisplayPowerMode value) {
		switch (value) {
			case DisplayPowerMode::On   : return lexxos::drivers::core::Ili9488PowerMode::On;
			case DisplayPowerMode::Off  : return lexxos::drivers::core::Ili9488PowerMode::Off;
			case DisplayPowerMode::Sleep: return lexxos::drivers::core::Ili9488PowerMode::Sleep;
		}

		return lexxos::drivers::core::Ili9488PowerMode::Invalid;
	};
	if (!lexxos::drivers::core::setIli9488Power(_commandTransport(), convert(_powerMode), convert(mode))) {
		return false;
	}

	_powerMode = mode;
	return true;
}

bool Ili9488Display::clear(std::uint16_t rgb565) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _frameActive) {
		return false;
	}

	std::optional<BusTransaction> transaction;
	if (!_acquireBus(transaction)) {
		return false;
	}

	if (!_setAddressWindow({.x = 0, .y = 0, .width = _config.width, .height = _config.height})) {
		return false;
	}

	_select();
	_backend.dataMode(true);
	_writeRepeatedPixel(rgb565, _config.width * _config.height);
	_deselect();
	return _ready;
}

bool Ili9488Display::beginFrame() {
	if (!_ready || _frameActive || _powerMode != DisplayPowerMode::On) {
		return false;
	}

	if (!_acquireBus(_frameTransaction)) {
		_frameTransaction.reset();
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

	// The RK3506 backend can feed SPI0 with native RGB565 halfwords from DDR.
	// Do not touch the generic byte-stream path or any RGB666 panel.
	if (_config.wireFormat == drivers::core::Ili9488PixelFormat::Rgb565 && _backend.supportsRgb565Words()) {
		bool ok = true;
		auto const pixelCount = static_cast<std::size_t>(rect.width) * rect.height;
		if (stridePixels == rect.width || rect.height == 1) {
			ok = _backend.writeRgb565Words(pixels, pixelCount);
		} else if (_buffers.pixels.size() >= pixelCount * sizeof(std::uint16_t)
		    && (reinterpret_cast<std::uintptr_t>(_buffers.pixels.data()) & 1U) == 0U) {
			// A strided rectangle cannot be passed as a single linear DMA source.
			// Stage only the dirty rectangle in the existing DDR wire workspace;
			// this avoids one IRQ/scheduler wakeup for every display row.
			auto const stageStarted = _counter ? _counter() : 0U;

			// Source and destination are halfword aligned. The RK3506
			// freestanding memcpy is byte-by-byte in Debug; use halfword
			// copies on this native RGB565-only DMA path.
			auto* target = reinterpret_cast<std::uint16_t*>(_buffers.pixels.data());
			for (int y = 0; y < rect.height; ++y) {
				auto const* row = pixels + static_cast<std::size_t>(y) * stridePixels;
				auto* output = target + static_cast<std::size_t>(y) * rect.width;
				for (int x = 0; x < rect.width; ++x) {
					output[x] = row[x];
				}
			}

			_timing.roiStageBytes += pixelCount * sizeof(std::uint16_t);
			++_timing.roiStageCalls;

			if (_counter) {
				_timing.roiStageTicks += _counter() - stageStarted;
			}

			ok = _backend.writeRgb565Words(
			    reinterpret_cast<std::uint16_t const*>(_buffers.pixels.data()), pixelCount);
		} else {
			// Unusually small/misaligned workspace: retain the proven row path.
			for (int y = 0; y < rect.height && ok; ++y) {
				ok = _backend.writeRgb565Words(pixels + static_cast<std::size_t>(y) * stridePixels, rect.width);
			}
		}

		if (!ok) {
			_ioFailed = true;
			_ready = false; // DMA may have emitted a partial frame; never retry it.
		}

		return endRgb565Stream();
	}

	// При достаточной ёмкости готовим весь прямоугольник, включая stride,
	// и передаём одним write. Малые workspace сохраняют построчный путь.
	auto const bytesPerPixel = static_cast<std::size_t>(wireBytesPerPixel());
	auto const rowBytes = static_cast<std::size_t>(rect.width) * bytesPerPixel;
	if (static_cast<std::size_t>(rect.height) <= _buffers.pixels.size() / rowBytes) {
		for (int y = 0; y < rect.height; ++y) {
			auto* output = _buffers.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
			auto const* input = pixels + static_cast<std::size_t>(y) * stridePixels;

			if (bytesPerPixel == 2) {
				drivers::core::fillRgb565WireFromRgb565Pixels(output, input, rect.width);
			} else {
				drivers::core::fillRgb888FromRgb565Pixels(output, input, rect.width);
			}
		}

		_writeBytes(_buffers.pixels.data(), rowBytes * rect.height);
		return endRgb565Stream();
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
	_backend.dataMode(true);
	_rgb565StreamActive = true;
	return true;
}

bool Ili9488Display::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || pixels == nullptr || count < 0) {
		return false;
	}
	_writePixels(pixels, count);
	return _ready;
}

bool Ili9488Display::writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}

	return lexxos::drivers::core::streamIli9488Pixels(_pixelTransport(),
	    _config.wireFormat,
	    lexxos::drivers::core::Ili9488PixelSource::Rgb565Bytes, bytes, pixelCount, _buffers.pixels, _buffers.pixels.size() / 3);
}

bool Ili9488Display::writeRgb888Stream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || !_frameActive || !_rgb565StreamActive || bytes == nullptr || pixelCount < 0) {
		return false;
	}

	return lexxos::drivers::core::streamIli9488Pixels(_pixelTransport(),
	    _config.wireFormat,
	    lexxos::drivers::core::Ili9488PixelSource::Rgb888Bytes, bytes, pixelCount, _buffers.pixels, _buffers.pixels.size() / 3);
}

bool Ili9488Display::endRgb565Stream() {
	if (!_frameActive || !_rgb565StreamActive) {
		return false;
	}

	_rgb565StreamActive = false;
	_deselect();
	return _ready;
}

bool Ili9488Display::configureVerticalScroll(int topFixedPixels, int scrollAreaPixels, int bottomFixedPixels) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _rgb565StreamActive || topFixedPixels < 0 || scrollAreaPixels <= 0 || bottomFixedPixels < 0) {
		return false;
	}

	std::optional<BusTransaction> transaction;
	if (!_acquireBus(transaction)) {
		return false;
	}

	auto const requestedHeightPixels = static_cast<std::int64_t>(topFixedPixels) + scrollAreaPixels + bottomFixedPixels;
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

	if (!lexxos::drivers::core::configureIli9488Scroll(_commandTransport(), topFixedPixels, scrollAreaPixels, bottomFixedPixels)) {
		return false;
	}

	_verticalScrollAreaPixels = scrollAreaPixels;
	_verticalScrollTopFixedPixels = topFixedPixels;
	return _ready;
}

bool Ili9488Display::setVerticalScrollStart(int startPixel) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _rgb565StreamActive || _verticalScrollAreaPixels <= 0) {
		return false;
	}

	std::optional<BusTransaction> transaction;
	if (!_acquireBus(transaction)) {
		return false;
	}

	auto normalized = startPixel % _verticalScrollAreaPixels;
	if (normalized < 0) {
		normalized += _verticalScrollAreaPixels;
	}

	auto const address = _verticalScrollTopFixedPixels + normalized;
	return lexxos::drivers::core::setIli9488ScrollStart(_commandTransport(), address);
}

bool Ili9488Display::endFrame() {
	if (!_frameActive) {
		return false;
	}

	if (_rgb565StreamActive) {
		endRgb565Stream();
	}

	_frameActive = false;
	_frameTransaction.reset();
	return _ready;
}

void Ili9488Display::_hardwareReset() {
	auto const& reset = _config.reset;
	if (!reset.enabled) {
		return;
	}

	_backend.resetLine(reset.activeLow);
	_backend.sleepMs(reset.beforeAssertMs);
	_backend.resetLine(!reset.activeLow);
	_backend.sleepMs(reset.assertedMs);
	_backend.resetLine(reset.activeLow);
	_backend.sleepMs(reset.recoveryMs);
}

lexxos::drivers::core::Ili9488CommandTransport Ili9488Display::_commandTransport() {
	return {
	    .context = this,
	    .write = [](void* context, std::uint8_t command, std::uint8_t const* data, std::size_t size) {
  auto& display = *static_cast<Ili9488Display*>(context);
  display._writeCommand(command);
  if (display._ioFailed) { return false; }
  if (size != 0) { display._writeData(data, size); }
  return !display._ioFailed; },
	    .sleepMs = [](void* context, std::uint32_t milliseconds) { static_cast<Ili9488Display*>(context)->_backend.sleepMs(milliseconds); },
	};
}

void Ili9488Display::_initializeController() {
	if (!lexxos::drivers::core::initializeIli9488(_commandTransport(),
	        _config.wireFormat, _config.invertColors)) {
		_ioFailed = true;
	}
}

void Ili9488Display::_select() {
	_backend.select(true);
}

void Ili9488Display::_deselect() {
	_backend.select(false);
}

lexxos::drivers::core::Ili9488PixelTransport Ili9488Display::_pixelTransport() {
	auto write = [](void* context, std::uint8_t const* data, std::size_t size) {
		auto& display = *static_cast<Ili9488Display*>(context);
		display._writeBytes(data, size);
		return !display._ioFailed;
	};
	return {this, write};
}

void Ili9488Display::_writePixels(std::uint16_t const* pixels, int count) {
	if (count <= 0) {
		return;
	}

	if (!lexxos::drivers::core::streamIli9488Pixels(_pixelTransport(),
	        _config.wireFormat,
	        lexxos::drivers::core::Ili9488PixelSource::Rgb565Words, pixels, count, _buffers.pixels, _buffers.pixels.size() / 3)) {
		_ioFailed = true;
		_ready = false;
	}
}

void Ili9488Display::_writeRepeatedPixel(std::uint16_t color, int count) {
	if (count <= 0) {
		return;
	}

	if (!lexxos::drivers::core::fillIli9488Pixels(_pixelTransport(),
	        _config.wireFormat,
	        color, count, _buffers.repeated, _buffers.repeated.size() / 3)) {
		_ioFailed = true;
		_ready = false;
	}
}

void Ili9488Display::_writeCommand(std::uint8_t command) {
	_select();
	_backend.dataMode(false);
	_writeBytes(&command, 1);
	_deselect();
}

void Ili9488Display::_writeData(std::uint8_t value) {
	_writeData(&value, 1);
}

void Ili9488Display::_writeData(std::uint8_t const* data, std::size_t size) {
	_select();
	_backend.dataMode(true);
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

	return lexxos::drivers::core::setIli9488Window(_commandTransport(), x0, y0, x1, y1);
}

bool Ili9488Display::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x <= _config.width && rect.y <= _config.height && rect.width <= _config.width - rect.x && rect.height <= _config.height - rect.y;
}

} // namespace lexxos::devices
