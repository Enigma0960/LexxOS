#pragma once
#include <cstddef>
#include <cstdint>

namespace lexxos::devices {

// Секторный транспорт SD; владение контроллером остаётся у composition.
class SdMediaTransport {
public:
	virtual ~SdMediaTransport();
	virtual bool ready() = 0;
	virtual bool inserted() = 0;
	virtual bool probe() = 0;

	virtual char const* failureStage() { return "unavailable"; }

	virtual std::uint8_t lastResponse() { return 0xFF; }

	virtual void invalidateRemoved() = 0;
	virtual std::uint64_t sectorCount() = 0;
	virtual bool read(std::uint32_t lba, std::uint8_t* data, std::size_t count) = 0;
	virtual bool write(std::uint32_t lba, std::uint8_t const* data, std::size_t count) = 0;
	virtual bool configured() = 0;
	virtual bool highCapacity() = 0;
	virtual std::uint32_t clockMHz() = 0;
	virtual bool setClockMHz(std::uint32_t value) = 0;
	virtual bool dmaAvailable() = 0;
	virtual bool dmaEnabled() = 0;
};

} // namespace lexxos::devices
