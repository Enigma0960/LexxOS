#include <lexxos/devices/display/double_buffered_display.h>

#include <algorithm>
#include <limits>

#include <lexxos/devices/display/rgb565_blend.h>

namespace lexxos::devices {

namespace {

	// Буферы RGB565 не перекрываются. Не понижаем копирование кадров до
	// побайтового freestanding memmove: этот модуль собирается с -O2.
	void copyPixels(std::uint16_t* target, std::uint16_t const* source, std::size_t count) {
		for (std::size_t i = 0; i < count; ++i) {
			target[i] = source[i];
		}
	}

} // namespace

DoubleBufferedDisplay::DoubleBufferedDisplay(DisplayDevice& panel, std::span<std::uint16_t> first,
    std::span<std::uint16_t> second, Wait wait, Counter counter, Notify notify, bool hardwareScrollEnabled, DisplayFrameSynchronization synchronization)
    : _synchronization(synchronization)
    , _panel(panel)
    , _buffers{first, second}
    , _wait(wait)
    , _counter(counter)
    , _notify(notify)
    , _hardwareScrollEnabled(hardwareScrollEnabled) {
}

DoubleBufferedDisplayTiming DoubleBufferedDisplay::timing() const {
	Guard guard{*this};
	return _timing;
}

bool DoubleBufferedDisplay::hasPendingFrame() const {
	Guard guard{*this};
	return _pending >= 0;
}

bool DoubleBufferedDisplay::initialize() {
	if ((_synchronization.enter == nullptr) != (_synchronization.leave == nullptr)) {
		return false;
	}

	if (_frame || hasPendingFrame() || !_panel.isReady() || width() <= 0 || height() <= 0) {
		return false;
	}

	auto const count = static_cast<std::uint64_t>(width()) * height();
	if (count > std::numeric_limits<int>::max() || _buffers[0].size() < count || _buffers[1].size() < count) {
		return false;
	}

	auto const a = reinterpret_cast<std::uintptr_t>(_buffers[0].data());
	auto const b = reinterpret_cast<std::uintptr_t>(_buffers[1].data());
	if (!a || !b || (a < b ? b - a : a - b) < count * sizeof(std::uint16_t)) {
		return false;
	}

	for (auto& buffer : _buffers) {
		buffer = buffer.first(static_cast<std::size_t>(count));
		std::fill(buffer.begin(), buffer.end(), 0);
	}

	_lastPublishedDirty.clear();
	_dirty.clear();
	_pendingDirty.clear();
	_forceFullSync = false;
	_backPrepared = false;
	_frameScrollStart = -1;
	_pendingScrollStart = -1;
	_timing = {};
	auto const ready = _panel.clear(0);
	Guard guard{*this};
	_ready = ready;
	return ready;
}

void DoubleBufferedDisplay::enableAsync() {
	Guard guard{*this};
	if (_wait) {
		_async = true;
	}
}

std::string_view DoubleBufferedDisplay::name() const {
	return _panel.name();
}

bool DoubleBufferedDisplay::isReady() const {
	Guard guard{*this};
	return _ready && _panel.isReady();
}

int DoubleBufferedDisplay::width() const {
	return _panel.width();
}

int DoubleBufferedDisplay::height() const {
	return _panel.height();
}

PixelFormat DoubleBufferedDisplay::nativeFormat() const {
	return PixelFormat::Rgb565;
}

DisplayWireFormat DoubleBufferedDisplay::wireFormat() const {
	return _panel.wireFormat();
}

int DoubleBufferedDisplay::wireBytesPerPixel() const {
	return _panel.wireBytesPerPixel();
}

std::uint32_t DoubleBufferedDisplay::busSpeedHz() const {
	return _panel.busSpeedHz();
}

bool DoubleBufferedDisplay::supportsPowerMode() const {
	return _panel.supportsPowerMode();
}

DisplayPowerMode DoubleBufferedDisplay::powerMode() const {
	return _panel.powerMode();
}

bool DoubleBufferedDisplay::waitIdle() {
	for (;;) {
		bool async, servicing;
		{
			Guard guard{*this};
			if (_pending < 0) {
				break;
			}

			async = _async;
			servicing = _servicing;
		}

		if (!async && !servicing) {
			if (!service()) {
				return false;
			}
		} else if (_wait) {
			_wait(1);
		} else {
			return false;
		}
	}

	return isReady();
}

bool DoubleBufferedDisplay::service() {
	DirtyRegionSet regions;
	int pending;
	int scrollStart;
	std::uint64_t serviceStarted{};
	{
		Guard guard{*this};
		if (_servicing || _pending < 0) {
			return _ready;
		}

		_servicing = true;
		serviceStarted = _counter ? _counter() : 0U;
		pending = _pending;
		regions = _pendingDirty;
		scrollStart = _pendingScrollStart;
		if (_counter) {
			_timing.queueTicks += serviceStarted - _queuedAt;
		}
	}

	// One panel transaction may contain several independently positioned ROIs.
	// Never hold the metadata critical section across SPI/DMA or a callback.
	bool ok = true;
	if (!regions.empty()) {
		ok = _panel.beginFrame();
		if (ok) {
			for (std::size_t i = 0; i < regions.size(); ++i) {
				auto const& rect = regions[i];
				auto const* pixels = _buffers[pending].data() + rect.y * width() + rect.x;
				if (!_panel.blitRgb565(rect, pixels, width())) {
					ok = false;
					break;
				}
			}

			// Complete the transaction even if a later rectangle failed.
			ok = _panel.endFrame() && ok;
		}
	}

	// Scroll register follows all changed pixels, never the other way around.
	if (ok && scrollStart >= 0) {
		ok = _panel.setVerticalScrollStart(scrollStart);
	}

	{
		Guard guard{*this};
		_ready = ok;
		if (_counter) {
			_timing.presentTicks += _counter() - serviceStarted;
		}

		if (ok) {
			++_timing.presentedFrames;
			_timing.presentedRegions += regions.size();
			_timing.presentedPixels += regions.pixels();
		}

		_pending = -1;
		_pendingScrollStart = -1;
		_servicing = false;
	}
	return ok;
}

bool DoubleBufferedDisplay::beginFrame() {
	if (!isReady() || _frame || powerMode() != DisplayPowerMode::On) {
		return false;
	}

	// Defer copying until the first write. A full-frame overwrite needs no
	// preservation of the previous image, even when DMA reads it concurrently.
	_backPrepared = false;
	_frameCopyTicks = 0U;
	_dirty.clear();
	_frameFailed = false;
	_frameScrollStart = -1;
	_frame = true;
	_drawStarted = _counter ? _counter() : 0U;
	return true;
}

void DoubleBufferedDisplay::prepareBackBuffer(Rect const& rect, bool fullyOverwritten) {
	if (_backPrepared) {
		return;
	}

	_backPrepared = true;
	if (fullyOverwritten && rect.x == 0 && rect.y == 0 && rect.width == width() && rect.height == height()) {
		Guard guard{*this};
		++_timing.skippedSyncs; // The entire next frame overwrites stale pixels.
		return;
	}

	// The previous frame may have changed multiple disjoint regions. Copy all
	// of them into the other framebuffer, or old sprite pixels will reappear.
	DirtyRegionSet sync = _lastPublishedDirty;
	if (_forceFullSync) {
		sync.clear();
		sync.add({0, 0, width(), height()});
	}

	if (sync.empty()) {
		Guard guard{*this};
		++_timing.skippedSyncs;
		return;
	}

	// The presenter only reads _latest; its DMA never writes this buffer.
	auto const started = _counter ? _counter() : 0U;

	for (std::size_t index = 0; index < sync.size(); ++index) {
		auto const& rect = sync[index];
		for (int y = rect.y; y < rect.y + rect.height; ++y) {
			auto const offset = static_cast<std::size_t>(y) * width() + rect.x;
			copyPixels(_buffers[_draw].data() + offset, _buffers[_latest].data() + offset, rect.width);
		}
	}

	Guard guard{*this};
	_timing.copiedBytes += sync.pixels() * sizeof(std::uint16_t);
	++_timing.copiedFrames;

	if (_forceFullSync) {
		++_timing.fullSyncs;
	} else {
		++_timing.partialSyncs;
	}

	if (_counter) {
		auto const elapsed = _counter() - started;
		_timing.copyTicks += elapsed;
		_frameCopyTicks += elapsed;
	}
}

bool DoubleBufferedDisplay::endFrame() {
	if (!_frame) {
		return false;
	}

	bool const complete = (!_stream || endRgb565Stream()) && !_frameFailed;

	if (complete && _counter) {
		Guard guard{*this};
		auto const elapsed = _counter() - _drawStarted;
		_timing.drawTicks += elapsed > _frameCopyTicks ? elapsed - _frameCopyTicks : 0U;
		++_timing.preparedFrames;
	}

	if (!complete || !waitIdle()) {
		// The unpublished back buffer may contain arbitrary partial writes.
		_forceFullSync = true;
		_frame = false;
		_frameScrollStart = -1;
		return false;
	}

	bool async, published = false;
	{
		Guard guard{*this};
		if (!_dirty.empty()) {
			_pendingDirty = _dirty;
			_latest = _draw;
			_lastPublishedDirty = _dirty;
			_forceFullSync = false;
			_queuedAt = _counter ? _counter() : 0U;
			_pendingScrollStart = _frameScrollStart;
			auto const completed = _draw;
			_draw ^= 1;
			_frame = false;
			_pending = completed; // Publish only after all producer state is complete.
			published = true;
		} else if (_frameScrollStart >= 0) {
			// A controller-scroll-only frame still needs presenter delivery.
			_pendingDirty.clear();
			_pendingScrollStart = _frameScrollStart;
			_queuedAt = _counter ? _counter() : 0U;
			_pending = _latest;
			published = true;
		} else if (_backPrepared) {
			_forceFullSync = true;
		}

		_frame = false;
		_frameScrollStart = -1;
		async = _async;
	}

	if (async) {
		if (published && _notify) {
			_notify();
		}

		return isReady();
	}

	return service();
}

bool DoubleBufferedDisplay::valid(Rect const& r) const {
	return r.x >= 0 && r.y >= 0 && r.width > 0 && r.height > 0 && r.x <= width() && r.y <= height() && r.width <= width() - r.x && r.height <= height() - r.y;
}

void DoubleBufferedDisplay::dirty(Rect const& r) {
	_dirty.add(r);
}

bool DoubleBufferedDisplay::clear(std::uint16_t color) {
	return beginFrame() && fillRgb565({0, 0, width(), height()}, color) && endFrame() && waitIdle();
}

bool DoubleBufferedDisplay::beginRgb565Stream(Rect const& r) {
	if (!isReady() || !_frame || _stream || !valid(r)) {
		return false;
	}

	prepareBackBuffer(r);

	_streamRect = r;
	_streamOffset = 0;
	_stream = true;
	return true;
}

bool DoubleBufferedDisplay::write(void const* source, int count, int sourceBytes) {
	if (!isReady() || !_frame || !_stream || !source || count < 0) {
		if (_frame && _stream) {
			_frameFailed = true;
		}

		return false;
	}

	auto const total = static_cast<std::size_t>(_streamRect.width) * _streamRect.height;
	if (static_cast<std::size_t>(count) > total - _streamOffset) {
		_frameFailed = true;
		return false;
	}

	auto const* bytes = static_cast<std::uint8_t const*>(source);
	int consumed = 0;
	while (consumed < count) {
		auto const column = _streamOffset % _streamRect.width;
		auto const row = _streamOffset / _streamRect.width;
		auto const run = std::min(static_cast<std::size_t>(count - consumed), static_cast<std::size_t>(_streamRect.width) - column);
		auto* target = _buffers[_draw].data() + (_streamRect.y + row) * width() + _streamRect.x + column;
		if (sourceBytes == 0) {
			copyPixels(target, static_cast<std::uint16_t const*>(source) + consumed, run);
		} else {
			for (std::size_t i = 0; i < run; ++i) {
				if (sourceBytes == 2) {
					target[i] = (std::uint16_t(bytes[0]) << 8) | bytes[1];
				} else {
					target[i] = ((bytes[0] & 0xF8U) << 8) | ((bytes[1] & 0xFCU) << 3) | (bytes[2] >> 3);
				}

				bytes += sourceBytes;
			}
		}

		_streamOffset += run;
		consumed += static_cast<int>(run);
	}

	return true;
}

bool DoubleBufferedDisplay::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	return write(pixels, count, 0);
}

bool DoubleBufferedDisplay::writeRgb565BytesStream(std::uint8_t const* bytes, int count) {
	return write(bytes, count, 2);
}

bool DoubleBufferedDisplay::writeRgb888Stream(std::uint8_t const* bytes, int count) {
	return write(bytes, count, 3);
}

bool DoubleBufferedDisplay::endRgb565Stream() {
	if (!_stream) {
		return false;
	}

	_stream = false;
	bool const complete = _streamOffset == static_cast<std::size_t>(_streamRect.width) * _streamRect.height;
	if (complete) {
		dirty(_streamRect);
	} else {
		_frameFailed = true;
	}

	return complete;
}

bool DoubleBufferedDisplay::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stride) {
	if (!pixels || stride < rect.width || !beginRgb565Stream(rect)) {
		return false;
	}

	for (int row = 0; row < rect.height; ++row) {
		if (!writeRgb565Stream(pixels + static_cast<std::size_t>(row) * stride, rect.width)) {
			_stream = false;
			_frameFailed = true;
			return false;
		}
	}

	return endRgb565Stream();
}

bool DoubleBufferedDisplay::blitRgb565Alpha(Rect const& rect, std::uint16_t const* pixels,
    std::uint8_t const* alpha, int stride) {
	if (!pixels || !alpha || stride < rect.width || !isReady() || !_frame || _stream || !valid(rect)) {
		return false;
	}

	// Even a full-screen translucent sprite must preserve the prior frame.
	prepareBackBuffer(rect, false);
	for (int y = 0; y < rect.height; ++y) {
		auto* dst = _buffers[_draw].data() + (rect.y + y) * width() + rect.x;
		auto const* src = pixels + static_cast<std::size_t>(y) * stride;
		auto const* a = alpha + static_cast<std::size_t>(y) * stride;
		for (int x = 0; x < rect.width; ++x) {
			dst[x] = blendRgb565(src[x], dst[x], a[x]);
		}
	}

	dirty(rect);
	return true;
}

bool DoubleBufferedDisplay::fillRgb565(Rect const& rect, std::uint16_t color) {
	if (!isReady() || !_frame || _stream || !valid(rect)) {
		return false;
	}

	prepareBackBuffer(rect);

	for (int y = rect.y; y < rect.y + rect.height; ++y) {
		std::fill_n(_buffers[_draw].data() + y * width() + rect.x, rect.width, color);
	}

	dirty(rect);
	return true;
}

bool DoubleBufferedDisplay::configureBusSpeed(std::uint32_t hz) {
	return !_frame && waitIdle() && _panel.configureBusSpeed(hz);
}

bool DoubleBufferedDisplay::setPowerMode(DisplayPowerMode mode) {
	return !_frame && waitIdle() && _panel.setPowerMode(mode);
}

bool DoubleBufferedDisplay::configureVerticalScroll(int top, int area, int bottom) {
	return _hardwareScrollEnabled && !_stream && waitIdle() && _panel.configureVerticalScroll(top, area, bottom);
}

bool DoubleBufferedDisplay::setVerticalScrollStart(int start) {
	if (!_hardwareScrollEnabled || _stream) {
		return false;
	}

	if (_frame) {
		if (!isReady()) {
			return false;
		}

		// The terminal updates its logical scroll origin during beginFrame().
		// Defer the physical ILI9488 register write to the presenter so pixels
		// and scroll offset are applied together, with no partially scrolled view.
		_frameScrollStart = start;
		return true;
	}

	return waitIdle() && _panel.setVerticalScrollStart(start);
}

} // namespace lexxos::devices
