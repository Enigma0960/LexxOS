#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::devices {

// Синхронный транспорт панели; caller владеет шиной на время всей операции.
// Backend и его ресурсы должны жить дольше использующего его дисплея.
class Ili9488Backend {
public:
	virtual ~Ili9488Backend();
	virtual bool initialize() = 0;
	virtual bool write(std::uint8_t const* data, std::size_t size) = 0;

	// Optional platform-specific zero-copy RGB565 path; never used for commands.
	virtual bool supportsRgb565Words() const { return false; }

	virtual bool writeRgb565Words(std::uint16_t const*, std::size_t) { return false; }

	virtual bool configureBusSpeed(std::uint32_t hz) = 0;
	virtual std::uint32_t busSpeedHz() const = 0;
	virtual bool hardwareSpi() const = 0;
	virtual void select(bool selected) = 0;
	virtual void dataMode(bool data) = 0;
	virtual void resetLine(bool high) = 0;
	virtual void sleepMs(std::uint32_t milliseconds) const = 0;
};

} // namespace lexxos::devices
