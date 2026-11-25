#include <lexxos/devices/rk3506/lcd_spi_controller.h>

#if defined(LEXXOS_RK3506_LCD_SPI_TEST)
extern "C" std::uint32_t lcd_spi_test_read(std::uintptr_t);
extern "C" void lcd_spi_test_write(std::uintptr_t, std::uint32_t);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t kSpi0 = 0xFF120000U;
	constexpr std::uintptr_t kCru = 0xFF9A0000U;
	constexpr std::uintptr_t kRmio = 0xFF910000U;
	constexpr std::uintptr_t kGpio0Ioc = 0xFF950000U;

	constexpr std::uintptr_t kControl0 = 0x00U;
	constexpr std::uintptr_t kControl1 = 0x04U;
	constexpr std::uintptr_t kEnable = 0x08U;
	constexpr std::uintptr_t kSlaveEnable = 0x0CU;
	constexpr std::uintptr_t kBaud = 0x10U;
	constexpr std::uintptr_t kTxLevel = 0x1CU;
	constexpr std::uintptr_t kStatus = 0x24U;
	constexpr std::uintptr_t kIrqMask = 0x2CU;
	constexpr std::uintptr_t kIrqClear = 0x38U;
	constexpr std::uintptr_t kDmaControl = 0x3CU;
	constexpr std::uintptr_t kVersion = 0x48U;
	constexpr std::uintptr_t kTxData = 0x400U;

	// DFS=8-bit, master mode, SPI mode 0, TX-only (MISO is not wired).
	constexpr std::uint32_t kControl0TxOnly = 1U | (1U << 10U) | (1U << 11U) | (1U << 13U) | (1U << 18U);

	// DFS16=2, BHT=0; EM_BIG and MSB-first are preserved.
	constexpr std::uint32_t kControl0TxOnly16 = (kControl0TxOnly & ~((1U << 13U) | 3U)) | 2U;
	constexpr std::uint32_t kBaudDivider = 2U;
	constexpr std::uint32_t kFifoThreshold = 28U; // FIFO depth 32.
	constexpr std::uint32_t kTimeoutMs = 1000U;

	// CLKSEL_CON34: SPI0 divider [7:4], source [9:8].
	// Never modify SPI1 fields [15:10].
	constexpr std::uintptr_t kSpiClockSelect = kCru + 0x388U;
	constexpr std::uint32_t kSpi0ClockMask = 0x3F0U;
	constexpr std::uint32_t kClockOsc24M = 0U;
	constexpr std::uint32_t kClockGpllDiv = 1U;

	extern "C" std::uint32_t rk3506_timer_ticks();
	extern "C" std::uint64_t rk3506_timer_counter();
	extern "C" std::uint32_t rk3506_timer_counter_frequency();

	std::uint32_t read32(std::uintptr_t address) {
#if defined(LEXXOS_RK3506_LCD_SPI_TEST)
		return lcd_spi_test_read(address);
#else
		return *reinterpret_cast<std::uint32_t const volatile*>(address);
#endif
	}

	void write32(std::uintptr_t address, std::uint32_t value) {
#if defined(LEXXOS_RK3506_LCD_SPI_TEST)
		lcd_spi_test_write(address, value);
#else
		*reinterpret_cast<std::uint32_t volatile*>(address) = value;
#endif
	}

	std::uint32_t spiRead(std::uintptr_t offset) {
		return read32(kSpi0 + offset);
	}

	void spiWrite(std::uintptr_t offset, std::uint32_t value) {
		write32(kSpi0 + offset, value);
	}

	void muxRmio(unsigned pin, std::uint32_t function) {
		// RK3506: per-pin RMIO registers start at +0x80.
		write32(kRmio + 0x80U + pin * 4U, (0x7FU << 16U) | function);
	}

	struct ClockProfile {
		std::uint32_t source{};
		std::uint32_t divider{};
	};

	bool selectClock(std::uint32_t hz, ClockProfile& profile) {
		// OSC = 24 MHz.
		// GPLL_DIV = 187.5 MHz, inherited from the Lyra U-Boot.
		// SPI BAUDR divides the resulting peripheral clock by two.
		switch (hz) {
			case 6000000U:
				profile = {kClockOsc24M, 2U};
				return true;
			case 12000000U:
				profile = {kClockOsc24M, 1U};
				return true;
			case 23437500U:
				profile = {kClockGpllDiv, 4U};
				return true;
			case 31250000U:
				profile = {kClockGpllDiv, 3U};
				return true;
			case 46875000U:
				profile = {kClockGpllDiv, 2U};
				return true;
			default:
				return false;
		}
	}

	bool applyClock(ClockProfile profile) {
		auto const fields = ((profile.divider - 1U) << 4U)
		    | (profile.source << 8U);
		write32(kSpiClockSelect, (kSpi0ClockMask << 16U) | fields);
		return (read32(kSpiClockSelect) & kSpi0ClockMask) == fields;
	}

} // namespace

bool LcdSpiController::fail() {
	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);
	_clockHz = 0;
	_ready = false;
	return false;
}

bool LcdSpiController::initialize(std::uint32_t clockHz) {
	if (_dma.sourceRetained()) {
		return false;
	}

	ClockProfile profile{};
	if (!selectClock(clockHz, profile)) {
		return false;
	}

	_ready = false;
	_rgb565Direct16 = false;
	_clockHz = 0;

	// CRU_CLKGATE_CON12 bits 10,11: enable PCLK_SPI0 and CLK_SPI0.
	write32(kCru + 0x830U, (1U << 26U) | (1U << 27U));

	// RM_IO6 MOSI (function 68), RM_IO7 CLK (function 67).
	muxRmio(6U, 68U);
	muxRmio(7U, 67U);
	// GPIO0_A6/A7 => RMIO function 7, leave A2/A3/A4 as GPIO.
	write32(kGpio0Ioc + 0x04U, 0xFF007700U);

	auto const version = spiRead(kVersion);
	if (version == 0U || version == 0xFFFFFFFFU) {
		return false;
	}

	spiWrite(kEnable, 0U);
	spiWrite(kSlaveEnable, 0U);

	if (!applyClock(profile)) {
		return fail();
	}

	spiWrite(kIrqMask, 0U);
	spiWrite(kDmaControl, 0U);
	spiWrite(kIrqClear, 0xFFFFFFFFU);
	spiWrite(kControl0, kControl0TxOnly);
	spiWrite(kControl1, 0xFFFFU);
	spiWrite(kBaud, kBaudDivider);
	spiWrite(kEnable, 1U);
	// Internal SER must be enabled; external LCD CS is GPIO0_A4.
	spiWrite(kSlaveEnable, 1U);

	_ready = true;
	_clockHz = clockHz;

	return true;
}

bool LcdSpiController::setClockHz(std::uint32_t clockHz) {
	if (_dma.sourceRetained()) {
		return false;
	}

	ClockProfile next{};
	if (!_ready || !selectClock(clockHz, next)) {
		return false;
	}

	if (clockHz == _clockHz) {
		return true;
	}

	// Speed changes must happen between transactions, with CS high.
	if ((spiRead(kStatus) & 1U) != 0U || spiRead(kTxLevel) != 0U) {
		return false;
	}

	ClockProfile previous{};
	if (!selectClock(_clockHz, previous)) {
		return false;
	}

	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);

	bool const ok = applyClock(next);
	if (!ok) {
		(void) applyClock(previous);
	}

	spiWrite(kBaud, kBaudDivider);
	spiWrite(kEnable, 1U);
	spiWrite(kSlaveEnable, 1U);

	if (ok) {
		_clockHz = clockHz;
	}

	return ok;
}

std::uint32_t LcdSpiController::clockHz() const {
	return _ready ? _clockHz : 0U;
}

bool LcdSpiController::setRgb565Direct16(bool enabled) {
	if (_dma.sourceRetained()) {
		return false;
	}

	if (!_ready || (spiRead(kStatus) & 1U) != 0U || spiRead(kTxLevel) != 0U || (enabled && !_dma.enabled())) {
		return false;
	}

	_rgb565Direct16 = enabled;
	return true;
}

bool LcdSpiController::rgb565Direct16Enabled() const {
	return _ready && _rgb565Direct16 && _dma.enabled();
}

bool LcdSpiController::writeImpl(std::uint8_t const* data, std::size_t count) {
	if (!_ready || (data == nullptr && count != 0U)) {
		return false;
	}

	// Команды остаются на PIO; крупные блоки пикселей используют DMA после preflight.
	// On DMA failure the transfer may be partial: never retry with PIO.
	if (count >= 512U && _dma.enabled()) {
		return _dma.write(data, count) ? true : fail();
	}

	auto lastProgress = rk3506_timer_ticks();
	std::size_t offset = 0;

	while (offset < count) {
		auto const queued = spiRead(kTxLevel);

		if (queued >= kFifoThreshold) {
			if (static_cast<std::uint32_t>(rk3506_timer_ticks() - lastProgress) >= kTimeoutMs) {
				return fail();
			}

			continue;
		}

		// Fill all available FIFO slots without reading TXFLR or the
		// millisecond timer after every byte. Leave four slots spare.
		auto const available = static_cast<std::size_t>(kFifoThreshold - queued);
		auto const remaining = count - offset;
		auto const burst = remaining < available ? remaining : available;
		for (std::size_t i = 0; i < burst; ++i) {
			spiWrite(kTxData, data[offset + i]);
		}

		offset += burst;
		lastProgress = rk3506_timer_ticks();
	}

	// The caller may deassert GPIO CS or toggle DC immediately afterwards.
	while ((spiRead(kStatus) & 1U) != 0U || spiRead(kTxLevel) != 0U) {
		if (static_cast<std::uint32_t>(rk3506_timer_ticks() - lastProgress) >= kTimeoutMs) {
			return fail();
		}
	}

	return true;
}

LcdSpi0Statistics LcdSpiController::statistics() const {
	auto result = _statistics;
	result.counterHz = rk3506_timer_counter_frequency();
	return result;
}

bool LcdSpiController::write(std::uint8_t const* data, std::size_t count) {
	if (_dma.sourceRetained()) {
		return false;
	}

	auto const start = rk3506_timer_counter();
	bool const ok = writeImpl(data, count);
	_statistics.elapsedTicks += rk3506_timer_counter() - start;
	++_statistics.calls;
	_statistics.requestedBytes += count;

	if (ok) {
		_statistics.completedBytes += count;
	} else {
		++_statistics.failures;
	}

	return ok;
}

bool LcdSpiController::writeRgb565Words(std::uint16_t const* pixels, std::size_t count) {
	if (_dma.sourceRetained()) {
		return false;
	}

	if (!rgb565Direct16Enabled() || !pixels || count == 0U || count > 320U * 320U || (reinterpret_cast<std::uintptr_t>(pixels) & 1U) != 0U || (spiRead(kStatus) & 1U) != 0U || spiRead(kTxLevel) != 0U) {
		return false;
	}

	auto const start = rk3506_timer_counter();
	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);
	spiWrite(kControl0, kControl0TxOnly16);
	spiWrite(kControl1, 0xFFFFU);
	spiWrite(kEnable, 1U);
	spiWrite(kSlaveEnable, 1U);

	// CS is controlled by GPIO0_A4 and remains asserted by the panel driver.
	// Blocking until both PL330 and the SPI FIFO drain keeps 'pixels' alive.
	bool const ok = _dma.write16(pixels, count);
	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);
	spiWrite(kControl0, kControl0TxOnly);
	spiWrite(kControl1, 0xFFFFU);

	if (ok) {
		spiWrite(kEnable, 1U);
		spiWrite(kSlaveEnable, 1U);
	} else {
		// Partial DMA is not retried. Leave SPI0 stopped; UART stays usable.
		_rgb565Direct16 = false;
		_ready = false;
		_clockHz = 0U;
	}

	++_statistics.calls;
	_statistics.requestedBytes += count * sizeof(std::uint16_t);
	_statistics.elapsedTicks += rk3506_timer_counter() - start;

	if (ok) {
		_statistics.completedBytes += count * sizeof(std::uint16_t);
	} else {
		++_statistics.failures;
	}

	return ok;
}

} // namespace lexxos::platform::rk3506
