#pragma once

#include <lexxos/devices/rk3506/picosd_diagnostics.h>
#include <lexxos/devices/rk3506/picosd_transport.h>

namespace lexxos::platform::rk3506 {

class SdProtocolIo;

// Product-owned session for the single removable socket. Callers serialize
// operations through SPI1 ownership. I/O is borrowed from composition;
// independent instances do not imply concurrent access to physical hardware.
class SdProtocol {
public:
	explicit SdProtocol(SdProtocolIo& io);
	SdProtocol(SdProtocol const&) = delete;
	SdProtocol& operator=(SdProtocol const&) = delete;
	PicoSdCrcMode crcMode();
	bool setCrcMode(PicoSdCrcMode mode);
	PicoSdReadProfile profileStatistics();
	bool resetProfileStatistics();
	bool cardInserted();
	void invalidateRemoved();
	bool probeAuto();
	bool ready();
	std::uint64_t sectorCount();
	bool readSector(std::uint32_t lba, std::uint8_t* destination);
	bool readSectors(std::uint32_t lba, std::uint8_t* destination, std::size_t count);
	bool writeSectors(std::uint32_t lba, std::uint8_t const* source, std::size_t count);
	void setMediaHooks(PicoSdMediaHooks hooks);
	PicoSdDiagnosticStatus diagnosticStatus();
	PicoSdRegisterStatus registerStatus();
	bool selectTransport(bool spi1);
	bool probeManual();

private:
	bool mediaMounted();
	void suppressAutoProbe();
	void cs(bool high);
	std::uint8_t transfer(std::uint8_t output);
	bool readData(std::uint8_t* destination, std::size_t count);
	void deselect();
	void select();
	void initializePins();
	bool command(std::uint8_t index, std::uint32_t arg, std::uint8_t crc,
	    std::uint8_t& r1);
	std::uint16_t crc16(std::uint8_t const* bytes, std::size_t size);
	std::uint16_t receiveDataCrc();
	bool readCapacity();
	bool probe();
	bool readSectorImpl(std::uint32_t lba, std::uint8_t* destination);
	bool writeSectorRaw(std::uint32_t lba, std::uint8_t const* data);
	bool stopMultipleRead();
	bool readMultiple(std::uint32_t lba, std::uint8_t* output, std::size_t count);
	SdProtocolIo& _io;
	PicoSdMediaHooks _mediaHooks{};
	PicoSdReadProfile _profile{};
	PicoSdCrcMode _crcMode{PicoSdCrcMode::Slicing4};

	bool _configured{};
	bool _ready{};
	bool _highCapacity{};
	std::uint32_t _ocr{};
	std::uint64_t _sectorCount{};
	char const* _step{"idle"};
	std::uint8_t _response{0xFF};
	bool _requestSpi1{}; // GPIO is the safe default; select using 'picosd mode spi1'.
	bool _spi1Active{};
	bool _spi1Fault{};
	bool _sdCsAsserted{}; // GPIO CS low: never retune SPI while selected.
	alignas(4) std::uint8_t _verify[512]{};
};

} // namespace lexxos::platform::rk3506
