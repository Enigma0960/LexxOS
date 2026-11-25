#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <lexxos/devices/bus/bus_transaction.h>
#include <lexxos/devices/display/ili9488_backend.h>
#include <lexxos/devices/display/ili9488_buffers.h>
#include <lexxos/devices/display/ili9488_panel.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/drivers/core/display/ili9488_stream.h>
#include <lexxos/drivers/core/display/ili9488_transport.h>

namespace lexxos::devices {

struct Ili9488DisplayTiming {
	std::uint64_t roiStageTicks{};
	std::uint64_t roiStageBytes{};
	std::uint32_t roiStageCalls{};
};

class Ili9488Display final : public DisplayDevice {
public:
	// Буферы должны жить дольше дисплея и не использоваться другим владельцем.
	Ili9488Display(Ili9488Panel config,
	    Ili9488Backend& backend, Ili9488Buffers buffers, BusTransactionQueue* queue = nullptr);
	~Ili9488Display() override;
	bool initialize();

	using Counter = std::uint64_t (*)();

	void setTimingCounter(Counter counter) { _counter = counter; }

	[[nodiscard]] Ili9488DisplayTiming timing() const { return _timing; }

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] PixelFormat nativeFormat() const override;
	[[nodiscard]] DisplayWireFormat wireFormat() const override;
	[[nodiscard]] int wireBytesPerPixel() const override;
	[[nodiscard]] std::uint32_t busSpeedHz() const override;
	bool configureBusSpeed(std::uint32_t hz) override;
	[[nodiscard]] bool supportsPowerMode() const override;
	[[nodiscard]] DisplayPowerMode powerMode() const override;

	bool setPowerMode(DisplayPowerMode mode) override;
	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool fillRgb565(Rect const& rect, std::uint16_t rgb565) override;
	bool beginRgb565Stream(Rect const& rect) override;
	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override;
	bool writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) override;
	bool writeRgb888Stream(std::uint8_t const* bytes, int pixelCount) override;
	bool endRgb565Stream() override;
	bool configureVerticalScroll(int topFixedPixels, int scrollAreaPixels, int bottomFixedPixels) override;
	bool setVerticalScrollStart(int startPixel) override;
	bool endFrame() override;

private:
	lexxos::drivers::core::Ili9488PixelTransport _pixelTransport();
	lexxos::drivers::core::Ili9488CommandTransport _commandTransport();
	bool _acquireBus(std::optional<BusTransaction>& transaction);
	void _hardwareReset();
	void _initializeController();
	void _select();
	void _deselect();
	void _writeBytes(std::uint8_t const* data, std::size_t size);
	void _writePixels(std::uint16_t const* pixels, int count);
	void _writeRepeatedPixel(std::uint16_t rgb565, int count);
	void _writeCommand(std::uint8_t command);
	void _writeData(std::uint8_t value);
	void _writeData(std::uint8_t const* data, std::size_t size);
	bool _setAddressWindow(Rect const& rect);
	[[nodiscard]] bool _isRectValid(Rect const& rect) const;
	Ili9488Panel _config;
	Ili9488Backend& _backend;
	Ili9488Buffers _buffers;
	BusTransactionQueue* _busQueue{nullptr};
	std::optional<BusTransaction> _frameTransaction;
	Counter _counter{};
	Ili9488DisplayTiming _timing{};
	bool _ioFailed{false};
	bool _ready{false};
	bool _frameActive{false};
	bool _rgb565StreamActive{false};
	int _verticalScrollAreaPixels{0};
	int _verticalScrollTopFixedPixels{0};
	DisplayPowerMode _powerMode{DisplayPowerMode::On};
};

} // namespace lexxos::devices
