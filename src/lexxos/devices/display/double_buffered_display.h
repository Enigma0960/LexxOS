#pragma once

#include <span>

#include <lexxos/devices/display/detail/display_frame_guard.h>
#include <lexxos/devices/display/dirty_region_set.h>
#include <lexxos/devices/display/double_buffered_display_types.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::devices {

// Optional CPU0 timing: values are accumulated raw counter ticks.
// Draw time is elapsed wall time and may include scheduler delays.

// Optional single-core task synchronization, supplied by composition.
// Both callbacks must be present or both absent (cooperative execution only).

// Два полных RGB565-кадра. Один producer и presenter работают
// на одном CPU; при вытеснении обязательны synchronization callbacks.
// Один producer; IRQ не меняет кадры. Буферы живут дольше адаптера.
class DoubleBufferedDisplay final : public DisplayDevice {
public:
	using Wait = void (*)(std::uint32_t);
	using Counter = std::uint64_t (*)();
	using Notify = void (*)();

	DoubleBufferedDisplay(DisplayDevice& panel, std::span<std::uint16_t> first,
	    std::span<std::uint16_t> second, Wait wait, Counter counter = nullptr, Notify notify = nullptr,
	    bool hardwareScrollEnabled = true, DisplayFrameSynchronization synchronization = {});

	[[nodiscard]] DoubleBufferedDisplayTiming timing() const;

	[[nodiscard]] bool hasPendingFrame() const;

	bool initialize();
	void enableAsync();
	bool service();
	bool waitIdle() override;
	std::string_view name() const override;
	bool isReady() const override;
	int width() const override;
	int height() const override;
	PixelFormat nativeFormat() const override;
	DisplayWireFormat wireFormat() const override;
	int wireBytesPerPixel() const override;
	std::uint32_t busSpeedHz() const override;
	bool configureBusSpeed(std::uint32_t hz) override;
	bool supportsPowerMode() const override;
	DisplayPowerMode powerMode() const override;
	bool setPowerMode(DisplayPowerMode mode) override;
	bool clear(std::uint16_t color) override;

	[[nodiscard]] bool supportsPartialUpdates() const override { return true; }

	bool beginFrame() override;
	bool endFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stride) override;
	bool blitRgb565Alpha(Rect const& rect, std::uint16_t const* pixels,
	    std::uint8_t const* alpha, int stride) override;
	bool fillRgb565(Rect const& rect, std::uint16_t color) override;

	bool prefersRgb565Stream() const override { return true; }

	bool beginRgb565Stream(Rect const& rect) override;
	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override;
	bool writeRgb565BytesStream(std::uint8_t const* bytes, int count) override;
	bool writeRgb888Stream(std::uint8_t const* bytes, int count) override;
	bool endRgb565Stream() override;
	bool configureVerticalScroll(int top, int area, int bottom) override;
	bool setVerticalScrollStart(int start) override;

private:
	using Guard = detail::DisplayFrameGuard;
	friend class detail::DisplayFrameGuard;

	bool valid(Rect const& rect) const;
	void dirty(Rect const& rect);
	bool write(void const* pixels, int count, int sourceBytes);
	void prepareBackBuffer(Rect const& rect, bool fullyOverwritten = true);

	DisplayFrameSynchronization _synchronization;
	DisplayDevice& _panel;
	std::span<std::uint16_t> _buffers[2];
	Wait _wait;
	Counter _counter{};
	Notify _notify{};
	bool _hardwareScrollEnabled;
	DoubleBufferedDisplayTiming _timing{};

	std::uint64_t _drawStarted{};
	std::uint64_t _frameCopyTicks{};
	std::uint64_t _queuedAt{};
	int _draw{0};
	int _latest{1};
	int _pending{-1};
	int _frameScrollStart{-1};
	int _pendingScrollStart{-1};
	bool _ready{};
	bool _async{};
	bool _servicing{};
	bool _frame{};
	bool _frameFailed{};
	bool _backPrepared{};
	bool _stream{};
	bool _forceFullSync{};
	std::size_t _streamOffset{};

	DirtyRegionSet _lastPublishedDirty{};
	DirtyRegionSet _dirty{};
	DirtyRegionSet _pendingDirty{};
	Rect _streamRect{};
};

} // namespace lexxos::devices
