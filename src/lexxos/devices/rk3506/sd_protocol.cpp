// Removable SD command protocol; physical routing belongs to the I/O backend.
#include <lexxos/devices/rk3506/sd_protocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_profile_ro.h>
#include <lexxos/devices/rk3506/sd_protocol_io.h>

#if defined(LEXXOS_RK3506_DMA_SAFE)
extern "C" void rk3506_console_write(char const*);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::size_t kSectorSize = 512;

	// The destructor closes a phase even on early return/error.
	struct CycleScope {
		std::uint64_t& total;
		SdProtocolIo& io;
		std::uint64_t start;

		CycleScope(SdProtocolIo& source, std::uint64_t& target)
		    : total(target)
		    , io(source)
		    , start(io.counter()) {}

		~CycleScope() { total += io.counter() - start; }

		CycleScope(CycleScope const&) = delete;
		CycleScope& operator=(CycleScope const&) = delete;
	};

	// SD CRC16-CCITT, polynomial 0x1021, initial CRC = 0.
	// The table is generated at compile time and occupies 512 bytes of ROM,
	// not the foreground task's stack or heap.
	constexpr std::uint16_t crc16TableEntry(std::uint32_t byte) {
		std::uint16_t crc = static_cast<std::uint16_t>(byte << 8U);
		for (unsigned bit = 0; bit < 8U; ++bit) {
			crc = static_cast<std::uint16_t>((crc << 1U) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
		}
		return crc;
	}

	constexpr auto makeCrc16Table() {
		std::array<std::uint16_t, 256> table{};
		for (std::uint32_t i = 0; i < table.size(); ++i) {
			table[i] = crc16TableEntry(i);
		}
		return table;
	}

	constexpr auto kCrc16Table = makeCrc16Table();

	// Four parallel 8-bit slices for CRC16-CCITT. Slice 0 is the standard
	// byte lookup; higher slices advance that contribution by zero bytes.
	// All 4*256 entries are generated at compile time (2 KiB ROM, no heap).
	constexpr auto makeCrc16Slices() {
		std::array<std::array<std::uint16_t, 256>, 4> slices{};
		slices[0] = kCrc16Table;
		for (std::size_t step = 1; step < slices.size(); ++step) {
			for (std::size_t b = 0; b < 256U; ++b) {
				auto const prev = slices[step - 1U][b];
				slices[step][b] = static_cast<std::uint16_t>((prev << 8U) ^ kCrc16Table[prev >> 8U]);
			}
		}

		return slices;
	}

	constexpr auto kCrc16Slices = makeCrc16Slices();

} // namespace

SdProtocol::SdProtocol(SdProtocolIo& io)
    : _io(io) {
}

bool SdProtocol::mediaMounted() {
	return _mediaHooks.mounted();
}

void SdProtocol::suppressAutoProbe() {
	_mediaHooks.suppress();
}

void SdProtocol::cs(bool high) {
	_io.gpioChipSelect(high);
	_sdCsAsserted = !high;
}

std::uint8_t SdProtocol::transfer(std::uint8_t output) {
	if (_spi1Active) {
		std::uint8_t input{};
		if (!_io.spiTransfer(output, input)) {
			if (!_spi1Fault) {
				_step = "SPI1-timeout";
			}

			_spi1Fault = true;
			return 0xFFU;
		}

		return input;
	}

	return _io.gpioTransfer(output);
}

// Fast payload path only; command, token, CRC and CMD12 keep byte transfers.
bool SdProtocol::readData(std::uint8_t* destination, std::size_t count) {
	if (_spi1Active) {
		// Never retry a partially consumed SD packet by switching to PIO.
		bool const dma = _io.dmaEnabled();
		if (!(dma ? _io.dmaReadData(destination, count)
		          : _io.spiReadData(destination, count))) {
			_spi1Fault = true;
			_step = dma ? "SPI1-DMA" : "SPI1-FIFO";
			return false;
		}

		return true;
	}

	for (std::size_t i = 0; i < count; ++i) {
		destination[i] = transfer(0xFFU);
	}

	return true;
}

void SdProtocol::deselect() {
	cs(true);
	(void) transfer(0xFFU);
}

void SdProtocol::select() {
	deselect();
	cs(false);
	(void) transfer(0xFFU);
}

void SdProtocol::initializePins() {
	_ready = false;
	_sectorCount = 0;

	_crcMode = PicoSdCrcMode::Slicing4; // Fastest validated implementation.
	_io.dmaReset(); // Experimental mode must never survive reprobe.

	if (_spi1Active) {
		_io.spiStop();
	}

	_spi1Active = false;
	_spi1Fault = false;

	_io.gpioConfigure();
	_configured = true;
	cs(true);
	for (unsigned i = 0; i != 12; ++i) {
		(void) transfer(0xFFU); // >= 80 clocks before CMD0
	}

	if (_requestSpi1) {
		// Keep the SD chip deselected while changing the clock source/pinmux.
		if (!_io.spiInitialize()) {
			_spi1Fault = true;
			_step = "SPI1-init";
			return;
		}

		_spi1Active = true;
	}
}

bool SdProtocol::command(std::uint8_t index, std::uint32_t arg, std::uint8_t crc,
    std::uint8_t& r1) {
	CycleScope timer(_io, _profile.commandCycles);

	if (index == 17U) {
		++_profile.cmd17;
	}

	if (index == 18U) {
		++_profile.cmd18;
	}

	select();
	(void) transfer(static_cast<std::uint8_t>(0x40U | index));
	(void) transfer(static_cast<std::uint8_t>(arg >> 24U));
	(void) transfer(static_cast<std::uint8_t>(arg >> 16U));
	(void) transfer(static_cast<std::uint8_t>(arg >> 8U));
	(void) transfer(static_cast<std::uint8_t>(arg));
	(void) transfer(crc);
	for (unsigned i = 0; i != 12; ++i) {
		r1 = transfer(0xFFU);
		if ((r1 & 0x80U) == 0U) {
			_response = r1;
			return true;
		}
	}

	r1 = _response = 0xFF;
	deselect();
	return false;
}

std::uint16_t SdProtocol::crc16(std::uint8_t const* bytes, std::size_t size) {
	CycleScope timer(_io, _profile.crcComputeCycles);
	std::uint16_t crc = 0;

	if (_crcMode == PicoSdCrcMode::Slicing4) {
		std::size_t i = 0;
		while (size - i >= 4U) {
			auto const b0 = static_cast<std::uint8_t>((crc >> 8U) ^ bytes[i]);
			auto const b1 = static_cast<std::uint8_t>(crc ^ bytes[i + 1U]);
			crc = static_cast<std::uint16_t>(
			    kCrc16Slices[3][b0] ^ kCrc16Slices[2][b1] ^ kCrc16Slices[1][bytes[i + 2U]] ^ kCrc16Slices[0][bytes[i + 3U]]);
			i += 4U;
		}

		for (; i < size; ++i) {
			auto const index = static_cast<std::uint8_t>((crc >> 8U) ^ bytes[i]);
			crc = static_cast<std::uint16_t>((crc << 8U) ^ kCrc16Table[index]);
		}
	} else if (_crcMode == PicoSdCrcMode::Table) {
		for (std::size_t i = 0; i < size; ++i) {
			auto const index = static_cast<std::uint8_t>((crc >> 8U) ^ bytes[i]);
			crc = static_cast<std::uint16_t>((crc << 8U) ^ kCrc16Table[index]);
		}
	} else {
		for (std::size_t i = 0; i < size; ++i) {
			crc ^= static_cast<std::uint16_t>(bytes[i]) << 8U;
			for (unsigned bit = 0; bit < 8; ++bit) {
				crc = static_cast<std::uint16_t>((crc << 1U) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
			}
		}
	}

	return crc;
}

// Reading the card-supplied CRC bytes is timed separately from CRC math.
std::uint16_t SdProtocol::receiveDataCrc() {
	CycleScope timer(_io, _profile.crcWireCycles);
	auto const hi = transfer(0xFFU);
	auto const lo = transfer(0xFFU);
	return static_cast<std::uint16_t>((std::uint16_t(hi) << 8U) | lo);
}

// CMD9 CSD: derive capacity before permitting arbitrary FAT sector reads.
bool SdProtocol::readCapacity() {
	_step = "CMD9";
	std::uint8_t r1 = 0xFF;
	if (!command(9, 0, 0x01U, r1) || r1 != 0U) {
		deselect();
		return false;
	}

	auto const start = _io.ticks();
	std::uint8_t token = 0xFF;
	unsigned polls = 0;
	auto const tokenCyclesStart = _io.counter();

	do {
		++_profile.tokenPolls;
		token = transfer(0xFFU);
		if (token != 0xFFU) {
			break;
		}
	} while (++polls < 500000U && static_cast<std::uint32_t>(_io.ticks() - start) < 1500U);

	_profile.tokenCycles += _io.counter() - tokenCyclesStart;
	if (token != 0xFEU || _spi1Fault) {
		++_profile.tokenErrors;
		_step = "CSD-token";
		deselect();
		return false;
	}

	std::uint8_t csd[16]{};
	for (auto& byte : csd) {
		byte = transfer(0xFFU);
	}

	auto const expectedCrc = receiveDataCrc();
	deselect();
	if (crc16(csd, sizeof(csd)) != expectedCrc) {
		++_profile.crcErrors;
		_step = "CSD-CRC16";
		return false;
	}

	std::uint64_t sectors = 0;
	if ((csd[0] >> 6U) == 1U) {
		// SDHC/SDXC: 512 KiB per C_SIZE unit.
		std::uint32_t const csize = (std::uint32_t(csd[7] & 0x3FU) << 16U)
		    | (std::uint32_t(csd[8]) << 8U)
		    | csd[9];
		sectors = (std::uint64_t(csize) + 1U) * 1024U;
	} else if ((csd[0] >> 6U) == 0U) {
		unsigned const readLen = csd[5] & 15U;
		unsigned const mult = ((csd[9] & 3U) << 1U) | (csd[10] >> 7U);
		std::uint32_t const csize = (std::uint32_t(csd[6] & 3U) << 10U)
		    | (std::uint32_t(csd[7]) << 2U)
		    | (csd[8] >> 6U);
		if (readLen < 9U || readLen > 11U) {
			_step = "CSD-length";
			return false;
		}

		sectors = (std::uint64_t(csize) + 1U) << (mult + 2U + readLen - 9U);
	} else {
		_step = "CSD-version";
		return false;
	}

	if (sectors == 0U || sectors > UINT32_MAX || (!_highCapacity && sectors > (std::uint64_t(UINT32_MAX) + 1U) / kSectorSize)) {
		_step = "CSD-capacity";
		return false;
	}

	_sectorCount = sectors;
	return true;
}

bool SdProtocol::probe() {
	initializePins();

	if (_spi1Fault) {
		return false;
	}

	_step = "CMD0";
	std::uint8_t r1 = 0xFFU;
	// Some cards need repeated CMD0 during slow software-SPI startup.
	bool idle = false;
	for (unsigned n = 0; n < 5; ++n) {
		if (command(0, 0, 0x95U, r1) && r1 == 1U) {
			idle = true;
			break;
		}
	}

	deselect();
	if (!idle) {
		return false;
	}

	_step = "CMD8";
	if (!command(8, 0x1AAU, 0x87U, r1) || r1 != 1U) {
		deselect();
		return false; // First stage intentionally supports SD v2+ only.
	}

	std::uint32_t r7 = 0;
	for (unsigned i = 0; i < 4; ++i) {
		r7 = (r7 << 8U) | transfer(0xFFU);
	}

	deselect();
	if ((r7 & 0xFFFU) != 0x1AAU) {
		return false;
	}

	_step = "ACMD41";
	std::uint32_t const start = _io.ticks();
	unsigned attempts = 0;
	do {
		if (!command(55, 0U, 0x01U, r1) || r1 > 1U) {
			deselect();
			return false;
		}

		deselect();
		if (!command(41, 0x40000000U, 0x01U, r1)) {
			deselect();
			return false;
		}

		deselect();
		if (r1 == 0U) {
			break;
		}

		if (r1 != 1U) {
			return false;
		}
	} while (++attempts < 2000U && static_cast<std::uint32_t>(_io.ticks() - start) < 3000U);

	if (r1 != 0U) {
		return false;
	}

	_step = "CMD58";
	if (!command(58, 0U, 0x01U, r1) || r1 != 0U) {
		deselect();
		return false;
	}

	_ocr = 0;
	for (unsigned i = 0; i < 4; ++i) {
		_ocr = (_ocr << 8U) | transfer(0xFFU);
	}

	deselect();
	if ((_ocr & 0x80000000U) == 0U) {
		return false;
	}

	_highCapacity = (_ocr & (1U << 30U)) != 0;
	if (!_highCapacity) {
		_step = "CMD16";
		if (!command(16, kSectorSize, 0x01U, r1) || r1 != 0U) {
			deselect();
			return false;
		}

		deselect();
	}

	if (!readCapacity()) {
		return false;
	}

	if (_spi1Active) {
		_io.spiFastClock(); // 24 MHz / 4 = 6 MHz after card enumeration.
	}

	_ready = true;
	_step = "ready";

	return true;
}

bool SdProtocol::readSectorImpl(std::uint32_t lba, std::uint8_t* destination) {
	if (!_ready || !destination || lba >= _sectorCount || (!_highCapacity && lba > UINT32_MAX / kSectorSize)) {
		return false;
	}

	_step = "CMD17";
	std::uint32_t const argument = _highCapacity ? lba : lba * kSectorSize;
	if (!_highCapacity && lba > UINT32_MAX / kSectorSize) {
		return false;
	}

	std::uint8_t r1 = 0xFF;
	if (!command(17, argument, 0x01U, r1) || r1 != 0U) {
		deselect();
		return false;
	}

	_step = "data-token";
	std::uint32_t const start = _io.ticks();
	std::uint8_t token;
	unsigned polls = 0;
	auto const tokenCyclesStart = _io.counter();

	do {
		++_profile.tokenPolls;
		token = transfer(0xFFU);
		if (token != 0xFFU) {
			break;
		}
	} while (++polls < 500000U && static_cast<std::uint32_t>(_io.ticks() - start) < 1500U);

	_profile.tokenCycles += _io.counter() - tokenCyclesStart;
	if (token != 0xFEU || _spi1Fault) {
		++_profile.tokenErrors;
		deselect();
		return false;
	}

	_step = "data";
	if (!readData(destination, kSectorSize)) {
		deselect();
		return false;
	}

	auto const expectedCrc = receiveDataCrc();
	deselect();
	if (_spi1Fault) {
		return false;
	}

	if (crc16(destination, kSectorSize) != expectedCrc) {
		++_profile.crcErrors;
		_step = "data-CRC16";
		return false;
	}

	++_profile.goodSectors;
	_step = "ready";

	return true;
}

// CMD24 single-block writer backing the FatFs BlockDevice adapter.
// Data Response, busy timeout, CMD13 and CRC-checked readback are required.
bool SdProtocol::writeSectorRaw(std::uint32_t lba, std::uint8_t const* data) {
	if (!_ready || !_configured || _sdCsAsserted || _spi1Fault || data == nullptr || lba >= _sectorCount || (!_highCapacity && lba > UINT32_MAX / kSectorSize)) {
		return false;
	}

	if (!_io.gpioCardInserted()) {
		_step = "CMD24-removed";
		return false;
	}

	auto const failed = [&](char const* stage) {
		_step = stage;
		deselect();
		return false;
	};

	_step = "CMD24";
	std::uint8_t r1 = 0xFFU;
	auto const argument = _highCapacity ? lba : lba * kSectorSize;
	if (!command(24U, argument, 0x01U, r1) || r1 != 0U || _spi1Fault) {
		return failed("CMD24-R1");
	}

	// Nwr is satisfied by a dummy byte before the 0xFE start token.
	(void) transfer(0xFFU);
	(void) transfer(0xFEU);
	for (std::size_t i = 0; i < kSectorSize; ++i) {
		(void) transfer(data[i]);
		if (_spi1Fault || ((i & 63U) == 0U && !_io.gpioCardInserted())) {
			return failed("CMD24-payload/removed");
		}
	}

	// Always transmit valid CRC16, even when the card's SPI CRC option is off.
	auto const crc = crc16(data, kSectorSize);
	(void) transfer(static_cast<std::uint8_t>(crc >> 8U));
	(void) transfer(static_cast<std::uint8_t>(crc));
	if (_spi1Fault) {
		return failed("CMD24-CRC-wire");
	}

	// The card must answer with Data Response 0bxxx00101 (accepted).
	std::uint8_t response = 0xFFU;
	for (unsigned i = 0U; i < 16U; ++i) {
		response = transfer(0xFFU);
		if (response != 0xFFU || _spi1Fault) {
			break;
		}
	}

	if (_spi1Fault || (response & 0x1FU) != 0x05U) {
		return failed("CMD24-data-response");
	}

	// Busy is driven low until the internal flash program completes.
	auto const start = _io.ticks();
	bool released = false;
	for (unsigned i = 0U; i < 3000000U; ++i) {
		auto const value = transfer(0xFFU);
		if (_spi1Fault || !_io.gpioCardInserted()) {
			return failed("CMD24-busy/removed");
		}

		if (value == 0xFFU) {
			released = true;
			break;
		}

		if (static_cast<std::uint32_t>(_io.ticks() - start) >= 1500U) {
			break;
		}
	}

	if (!released) {
		return failed("CMD24-busy-timeout");
	}

	deselect();

	// CMD13 (R2): catch internal programming/write-protection errors.
	_step = "CMD13-after-write";
	if (!command(13U, 0U, 0x01U, r1) || r1 != 0U || _spi1Fault) {
		return failed("CMD24-CMD13-R1");
	}

	std::uint8_t const r2 = transfer(0xFFU);
	deselect();
	if (_spi1Fault || r2 != 0U) {
		_step = "CMD24-CMD13-R2";
		return false;
	}

	_step = "ready";
	return true;
}

// SPI-mode STOP_TRANSMISSION: CMD12 must be issued with CS still asserted.
// CMD12 has one mandatory stuff byte before its R1 response.
bool SdProtocol::stopMultipleRead() {
	CycleScope timer(_io, _profile.stopCycles);
	++_profile.cmd12;

	_step = "CMD12";
	(void) transfer(0x4CU);
	for (unsigned i = 0; i < 4; ++i) {
		(void) transfer(0x00U);
	}

	(void) transfer(0x01U);
	(void) transfer(0xFFU); // Stuff byte.
	std::uint8_t r1 = 0xFFU;
	bool replied = false;

	for (unsigned i = 0; i < 16U; ++i) {
		r1 = transfer(0xFFU);
		if ((r1 & 0x80U) == 0U) {
			replied = true;
			break;
		}
	}

	auto const start = _io.ticks();
	bool released = false;
	for (unsigned i = 0; i < 100000U; ++i) {
		if (transfer(0xFFU) == 0xFFU) {
			released = true;
			break;
		}

		if (std::uint32_t(_io.ticks() - start) >= 500U) {
			break;
		}
	}

	deselect();

	if (!replied || r1 != 0U || !released || _spi1Fault) {
		_step = "CMD12-failed";
		return false;
	}

	return true;
}

// CMD18 is only used for bounded contiguous runs. Every sector is
// CRC16-checked and CMD12 is sent even after a data/CRC failure.
bool SdProtocol::readMultiple(std::uint32_t lba, std::uint8_t* output, std::size_t count) {
	if (!_ready || output == nullptr || count < 2U || count > 8U || std::uint64_t(lba) + count > _sectorCount || (!_highCapacity && lba > UINT32_MAX / kSectorSize)) {
		return false;
	}

	_step = "CMD18";
	auto const argument = _highCapacity ? lba : lba * kSectorSize;
	std::uint8_t r1 = 0xFF;
	if (!command(18U, argument, 0x01U, r1) || r1 != 0U) {
		deselect();
		return false;
	}

	bool good = true;
	for (std::size_t block = 0; block < count; ++block) {
		_step = "CMD18-data-token";
		auto const started = _io.ticks();
		std::uint8_t token = 0xFFU;
		unsigned polls = 0;
		auto const tokenCyclesStart = _io.counter();

		do {
			++_profile.tokenPolls;
			token = transfer(0xFFU);
			if (token != 0xFFU) {
				break;
			}
		} while (++polls < 500000U && std::uint32_t(_io.ticks() - started) < 1500U);

		_profile.tokenCycles += _io.counter() - tokenCyclesStart;
		if (token != 0xFEU || _spi1Fault) {
			++_profile.tokenErrors;
			good = false;
			break;
		}

		auto* bytes = output + block * kSectorSize;
		if (!readData(bytes, kSectorSize)) {
			good = false;
			break;
		}

		auto const expectedCrc = receiveDataCrc();
		if (_spi1Fault) {
			good = false;
			break;
		}

		if (crc16(bytes, kSectorSize) != expectedCrc) {
			++_profile.crcErrors;
			good = false;
			break;
		}

		++_profile.goodSectors;
	}

	// A CRC/token failure still sends CMD12. On a hardware FIFO fault
	// the controller is disabled, so we can only release GPIO CS.
	bool const stopped = !_spi1Fault ? stopMultipleRead() : (deselect(), false);
	if (!good) {
		if (!_spi1Fault) {
			_step = "CMD18-data/CRC16";
		}

		return false;
	}

	if (!stopped) {
		return false;
	}

	_step = "ready";
	return true;
}

PicoSdCrcMode SdProtocol::crcMode() {
	return _crcMode;
}

bool SdProtocol::setCrcMode(PicoSdCrcMode mode) {
	if (!_ready || _sdCsAsserted || _spi1Fault || (mode != PicoSdCrcMode::Bitwise && mode != PicoSdCrcMode::Table && mode != PicoSdCrcMode::Slicing4)) {
		return false;
	}
	_crcMode = mode;
	return true;
}

PicoSdReadProfile SdProtocol::profileStatistics() {
	return _profile;
}

bool SdProtocol::resetProfileStatistics() {
	if (!_ready || _sdCsAsserted || _spi1Fault) {
		return false;
	}
	_profile = {};
	return true;
}

// Card detect is active-low and independent of the SD SPI data path.
// The GPIO1 clock may still be gated when the hotplug task starts at boot.
bool SdProtocol::cardInserted() {
	_io.gpioEnableClock();
	return _io.gpioCardInserted();
}

void SdProtocol::invalidateRemoved() {
	_ready = false;
	_sectorCount = 0;
	_step = "removed";
	_io.dmaReset();

	if (_spi1Active) {
		_io.spiStop();
	}

	_spi1Active = false;
	_spi1Fault = false;
	_configured = false;
}

// Read-only production profile. No secure GIC IRQ reconfiguration is attempted.
// A failed DMA probe falls back to the working SPI1 PIO reader.
bool SdProtocol::probeAuto() {
	if (!cardInserted() || mediaMounted()) {
		return false;
	}

	_requestSpi1 = true;
	if (!probe() || !ready()) {
		return false;
	}

	if (!_io.spiSetClockMHz(12U)) {
		_step = "SPI1-12MHz";
		_ready = false;
		return false;
	}

	if (_io.dmaProbe()) {
#if defined(LEXXOS_RK3506_DMA_SAFE)
		// FAT cache is WBWA: avoid trying a direct DMA destination on
		// every sector; the NC bounce buffer is always the safe default.
		bool const direct = false;
#else
		bool const direct = true;
#endif
		if (!(_io.dmaSetDirect(direct) && _io.dmaSetEnabled(true))) {
			(void) _io.dmaSetEnabled(false);
		}
	}
#if defined(LEXXOS_RK3506_DMA_SAFE)
	rk3506_console_write(_io.dmaEnabled()
	        ? "[sd] SPI1 PL330 DMA enabled (Normal-NC bounce)\n"
	        : "[sd] SPI1 PL330 unavailable; PIO fallback\n");
#endif

	return true;
}

bool SdProtocol::ready() {
	return _ready && _configured && _io.gpioCardInserted();
}

std::uint64_t SdProtocol::sectorCount() {
	return ready() ? _sectorCount : 0;
}

bool SdProtocol::readSector(std::uint32_t lba, std::uint8_t* destination) {
	if (!ready() || !readSectorImpl(lba, destination)) {
		++_profile.readErrors;
		_ready = false; // Fail closed: no stale data after a read/CRC error.
		return false;
	}

	return true;
}

bool SdProtocol::readSectors(std::uint32_t lba, std::uint8_t* destination, std::size_t count) {
	if (!ready() || destination == nullptr || count == 0U || std::uint64_t(lba) + count > _sectorCount) {
		return false;
	}

	while (count > 0U) {
		auto const batch = count > 8U ? 8U : count;
		bool const ok = batch == 1U
		    ? readSectorImpl(lba, destination)
		    : readMultiple(lba, destination, batch);
		if (!ok) {
			++_profile.readErrors;
			_ready = false; // Do not expose potentially stale/partial SD data.
			return false;
		}

		lba += static_cast<std::uint32_t>(batch);
		destination += batch * kSectorSize;
		count -= batch;
	}

	return true;
}

// FAT32 write path: CMD24 + full CRC16-checked sector readback.
// A multi-sector error may leave partial metadata: fail closed rather than
// retrying unsafe FAT writes automatically.
bool SdProtocol::writeSectors(std::uint32_t lba, std::uint8_t const* source, std::size_t count) {
	if (!ready() || source == nullptr || count == 0U || std::uint64_t(lba) + count > _sectorCount || _sdCsAsserted || _spi1Fault) {
		return false;
	}

	// Instance-owned readback scratch avoids task-stack growth.
	// Caller holds the physical SPI1 transaction for the complete operation.
	for (std::size_t sector = 0; sector < count; ++sector) {
		auto const current = lba + static_cast<std::uint32_t>(sector);
		auto const* bytes = source + sector * kSectorSize;

		if (!ready() || !writeSectorRaw(current, bytes) || !ready() || !readSectorImpl(current, _verify)) {
			_ready = false;
			suppressAutoProbe();
			return false;
		}

		for (std::size_t i = 0; i < kSectorSize; ++i) {
			if (_verify[i] != bytes[i]) {
				_step = "CMD24-readback-mismatch";
				_ready = false;
				suppressAutoProbe();
				return false;
			}
		}
	}

	return true;
}

void SdProtocol::setMediaHooks(PicoSdMediaHooks hooks) {
	_mediaHooks = hooks;
}

PicoSdDiagnosticStatus SdProtocol::diagnosticStatus() {
	return {_configured, _ready, _highCapacity, _ocr, _sectorCount, _step, _response, _requestSpi1, _spi1Active, _spi1Fault, _sdCsAsserted, _crcMode};
}

PicoSdRegisterStatus SdProtocol::registerStatus() {
	auto const snapshot = _io.gpioRegisters(_configured);
	return {snapshot.gpioGate, snapshot.spiGate, snapshot.iocC, snapshot.iocD,
	    snapshot.detectReadable, snapshot.detectHigh};
}

bool SdProtocol::selectTransport(bool spi1) {
	if (mediaMounted()) {
		return false;
	}

	suppressAutoProbe();
	_requestSpi1 = spi1;
	_ready = false;
	_sectorCount = 0;
	return true;
}

bool SdProtocol::probeManual() {
	return !mediaMounted() && probe();
}

} // namespace lexxos::platform::rk3506
