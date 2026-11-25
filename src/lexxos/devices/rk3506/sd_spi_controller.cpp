/* PicoCalc secondary microSD: RK3506 SPI1 PIO, SD mode 0, 8-bit frames.
 * Boot SDMMC (0xff480000) is not accessed. RM_IO30 remains GPIO CS so
 * >= 80 startup clocks can be issued with the card deselected.
 * Derived from Rockchip RK3506 clock/pinctrl DTS and spi-rockchip.c.
 */
#include <lexxos/devices/rk3506/sd_spi_controller.h>

#include <cstdint>

#if defined(LEXXOS_RK3506_SD_SPI_TEST)
extern "C" std::uint32_t sd_spi_test_read(std::uintptr_t);
extern "C" void sd_spi_test_write(std::uintptr_t, std::uint32_t);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t kSpi = 0xFF130000U;
	constexpr std::uintptr_t kCru = 0xFF9A0000U;
	constexpr std::uintptr_t kIoc = 0xFF660000U;
	constexpr std::uintptr_t kRmio = 0xFF910000U;

	constexpr std::uintptr_t kCtrl0 = 0x00U;
	constexpr std::uintptr_t kCtrl1 = 0x04U;
	constexpr std::uintptr_t kEnable = 0x08U;
	constexpr std::uintptr_t kSlaveEnable = 0x0CU;
	constexpr std::uintptr_t kBaud = 0x10U;
	constexpr std::uintptr_t kTxFifoLevel = 0x1CU;
	constexpr std::uintptr_t kRxFifoLevel = 0x20U;
	constexpr std::uintptr_t kStatus = 0x24U;
	constexpr std::uintptr_t kInterruptMask = 0x2CU;
	constexpr std::uintptr_t kRawInterruptStatus = 0x34U;
	constexpr std::uintptr_t kInterruptClear = 0x38U;
	constexpr std::uintptr_t kDmaControl = 0x3CU;
	constexpr std::uintptr_t kVersion = 0x48U;
	constexpr std::uintptr_t kTxData = 0x400U;
	constexpr std::uintptr_t kRxData = 0x800U;
	constexpr std::uint32_t kBaudInit = 64U; // SPI1 parent 24 MHz, output 375 kHz.
	constexpr std::uint32_t kBaudFast = 4U; // 6 MHz after successful CMD9.
	constexpr std::uint32_t kBaud12MHz = 2U; // 12 MHz experiment, 24 MHz parent.
	constexpr std::uint32_t kControl0 = 1U | (1U << 10U) | (1U << 11U) | (1U << 13U);
	// CTRLR0: 8 bits/frame (DFS=1), CPOL=0, CPHA=0, full duplex,
	// MSB first, 8-bit bus, big endian byte order, master mode.

	constexpr std::uint32_t kFifoDepth = 32U; // RK3506 SPI1 conservative depth.
	constexpr std::uint32_t kOutstandingLimit = 24U; // RX FIFO headroom.
	constexpr std::uint32_t kReadTimeoutMs = 500U; // Bound an entire data block.
	constexpr std::uint32_t kRxOverflowIrq = 1U << 3U;
	constexpr std::uint32_t kFifoFaultIrqs = (1U << 1U) | (1U << 2U) | kRxOverflowIrq;

	extern "C" std::uint32_t rk3506_timer_ticks();
	extern "C" std::uint64_t rk3506_timer_counter();

	std::uint32_t read32(std::uintptr_t a) {
#if defined(LEXXOS_RK3506_SD_SPI_TEST)
		return sd_spi_test_read(a);
#else
		return *reinterpret_cast<std::uint32_t const volatile*>(a);
#endif
	}

	void write32(std::uintptr_t a, std::uint32_t v) {
#if defined(LEXXOS_RK3506_SD_SPI_TEST)
		sd_spi_test_write(a, v);
#else
		*reinterpret_cast<std::uint32_t volatile*>(a) = v;
#endif
	}

	void spiWrite(std::uintptr_t offset, std::uint32_t v) {
		write32(kSpi + offset, v);
	}

	std::uint32_t spiRead(std::uintptr_t offset) {
		return read32(kSpi + offset);
	}

	void muxRmio(std::uintptr_t reg, std::uint32_t function) {
		// RMIO register: [22:16] write-enable for seven function bits.
		write32(kRmio + reg, (0x7FU << 16U) | function);
	}

} // namespace

// Drop the entire transfer on a bus error. Disabling SSIENR flushes both
// FIFOs; the caller must deassert GPIO CS and reprobe before any new read.
bool SdSpiController::abortFifoRead(bool timeout, bool overflow) {
	_fifoStats.lastStatus = spiRead(kStatus);
	_fifoStats.lastRawIrq = spiRead(kRawInterruptStatus);
	if (timeout) {
		++_fifoStats.timeouts;
	} else if (overflow) {
		++_fifoStats.overflows;
	} else {
		++_fifoStats.syncErrors;
	}

	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);
	_initialized = false;
	return false;
}

bool SdSpiController::initialize() {
	_initialized = false;
	_fifoStats = {};
	_fifoMode = PicoSpi1FifoMode::Safe; // Every probe starts in the proven mode.

	// Enable PCLK_SPI1 (CLKGATE_CON12.12) and CLK_SPI1 (.13), not SDMMC.
	write32(kCru + 0x830U, (1U << 28U) | (1U << 29U));
	// CLKSEL_CON34: SPI1 source=24 MHz oscillator, divider=1.
	// SPI1 parent mux bits 15:14; divider bits 13:10.
	write32(kCru + 0x388U, 0xFC000000U);
	// SPI1 master uses RM_IO28 MOSI, RM_IO29 clock, RM_IO31 MISO.
	// Keep RM_IO30 as GPIO CS for microSD initialization.
	muxRmio(0xF0U, 73U); // GPIO1_C3 -> SPI1_MOSI
	muxRmio(0xF4U, 72U); // GPIO1_D1 -> SPI1_CLK
	muxRmio(0xFCU, 74U); // GPIO1_D3 -> SPI1_MISO
	write32(kIoc + 0x30U, 0xF0007000U); // GPIO1_C3, mux 7
	write32(kIoc + 0x38U, 0xF0F07070U); // GPIO1_D1/D3, mux 7; D2 stays GPIO

	std::uint32_t const version = spiRead(kVersion);
	if (version == 0U || version == 0xFFFFFFFFU) {
		return false;
	}

	spiWrite(kEnable, 0U);
	spiWrite(kSlaveEnable, 0U);
	spiWrite(kInterruptMask, 0U);
	spiWrite(kDmaControl, 0U);
	spiWrite(kInterruptClear, 0xFFFFFFFFU);
	spiWrite(kCtrl0, kControl0);
	// Rockchip CTRLR1 frame count is not used to limit clocks in TR mode.
	spiWrite(kCtrl1, 0xFFFFU);
	spiWrite(kBaud, kBaudInit);
	spiWrite(kEnable, 1U);
	// SER is the controller's internal slave enable; physical CS is GPIO.
	spiWrite(kSlaveEnable, 1U);
	_initialized = true;
	return true;
}

void SdSpiController::stop() {
	if (!_initialized) {
		return;
	}

	spiWrite(kSlaveEnable, 0U);
	spiWrite(kEnable, 0U);
	_initialized = false;
}

bool SdSpiController::transfer(std::uint8_t tx, std::uint8_t& rx) {
	if (!_initialized) {
		return false;
	}

	std::uint32_t const start = rk3506_timer_ticks();
	// A full-duplex transfer must drain one received frame per sent frame.
	while (spiRead(kTxFifoLevel) >= 32U || spiRead(kRxFifoLevel) != 0U) {
		// An old byte from a previous aborted transaction is unusable.
		if (spiRead(kRxFifoLevel) != 0U) {
			(void) spiRead(kRxData);
		}

		if (std::uint32_t(rk3506_timer_ticks() - start) >= 20U) {
			return false;
		}
	}

	spiWrite(kTxData, tx);
	while (spiRead(kRxFifoLevel) == 0U) {
		if (std::uint32_t(rk3506_timer_ticks() - start) >= 20U) {
			return false;
		}
	}

	rx = static_cast<std::uint8_t>(spiRead(kRxData));
	// No DMA, no IRQ: bound the wait and let the next byte be queued.
	return true;
}

// Receive a data payload using clocks generated by TX dummy bytes. The number
// of sent-but-not-received frames never exceeds 24 (less than RX depth 32).
// Drain RX before each TX refill; no response byte is silently discarded.
bool SdSpiController::readData(std::uint8_t* destination, std::size_t count) {
	if (!_initialized || destination == nullptr || count == 0U) {
		return false;
	}

	if (spiRead(kTxFifoLevel) != 0U || spiRead(kRxFifoLevel) != 0U) {
		return abortFifoRead(false, false);
	}

	++_fifoStats.reads;
	auto const started = rk3506_timer_ticks();
	auto const startedCycles = rk3506_timer_counter();
	std::size_t queued = 0U;
	std::size_t received = 0U;
	bool const fast = _fifoMode == PicoSpi1FifoMode::Fast;
	std::uint32_t polls = 0U;

	while (received < count) {
		// Error bits are sticky. FAST samples every 16 loops and always
		// checks once more before handing the data to the CRC layer.
		if (!fast || (polls & 15U) == 0U) {
			++_fifoStats.irqChecks;
			auto const rawIrq = spiRead(kRawInterruptStatus);
			if ((rawIrq & kFifoFaultIrqs) != 0U) {
				_fifoStats.bytes += static_cast<std::uint32_t>(received);
				return abortFifoRead(false, (rawIrq & kRxOverflowIrq) != 0U);
			}
		}

		++polls;

		// Read only frames for which a corresponding TX dummy was queued.
		std::uint32_t const ready = spiRead(kRxFifoLevel);
		++_fifoStats.rxPolls;

		if (ready == 0U) {
			++_fifoStats.emptyRxPolls;
		} else {
			++_fifoStats.rxDrainBursts;
		}

		if (ready > kFifoDepth || ready > queued - received) {
			_fifoStats.bytes += static_cast<std::uint32_t>(received);
			return abortFifoRead(false, false);
		}

		if (ready > _fifoStats.peakRxFifo) {
			_fifoStats.peakRxFifo = ready;
		}

		for (std::uint32_t i = 0; i < ready; ++i) {
			destination[received++] = static_cast<std::uint8_t>(spiRead(kRxData));
		}

		if (received == count) {
			break;
		}

		// TXFLR <= queued-received: the FIFO can only contain frames that
		// have been queued but not yet received. At most 24 are outstanding,
		// so FAST always has at least eight free TX FIFO slots. SAFE keeps
		// the explicit 9g hardware check as an A/B reference.
		auto room = static_cast<std::size_t>(kOutstandingLimit) - (queued - received);
		if (!fast) {
			++_fifoStats.txLevelChecks;
			auto const txLevel = spiRead(kTxFifoLevel);
			if (txLevel > kFifoDepth) {
				_fifoStats.bytes += static_cast<std::uint32_t>(received);
				return abortFifoRead(false, false);
			}

			if (room > kFifoDepth - txLevel) {
				room = kFifoDepth - txLevel;
			}
		}

		if (room > count - queued) {
			room = count - queued;
		}

		if (room != 0U) {
			++_fifoStats.txBursts;
			for (std::size_t i = 0; i < room; ++i) {
				spiWrite(kTxData, 0xFFU);
			}

			queued += room;
			auto const pending = static_cast<std::uint32_t>(queued - received);
			if (pending > _fifoStats.peakPending) {
				_fifoStats.peakPending = pending;
			}
		}

		// Sampling the 1-ms timer every 16 loops bounds the additional
		// timeout latency while avoiding an MMIO timer access per poll.
		if ((!fast || (polls & 15U) == 0U) && std::uint32_t(rk3506_timer_ticks() - started) >= kReadTimeoutMs) {
			_fifoStats.bytes += static_cast<std::uint32_t>(received);
			return abortFifoRead(true, false);
		}
	}

	// FAST always checks the sticky error bits after the last byte.
	if (fast) {
		++_fifoStats.irqChecks;
		auto const rawIrq = spiRead(kRawInterruptStatus);
		if ((rawIrq & kFifoFaultIrqs) != 0U) {
			_fifoStats.bytes += static_cast<std::uint32_t>(received);
			return abortFifoRead(false, (rawIrq & kRxOverflowIrq) != 0U);
		}
	}

	_fifoStats.bytes += static_cast<std::uint32_t>(received);
	_fifoStats.payloadCycles += rk3506_timer_counter() - startedCycles;
	return true;
}

void SdSpiController::fastClock() {
	if (!_initialized) {
		return;
	}

	// No transaction is active: CMD9 has completed and the GPIO CS is high.
	std::uint32_t const start = rk3506_timer_ticks();
	while ((spiRead(kStatus) & 1U) != 0U) {
		if (std::uint32_t(rk3506_timer_ticks() - start) >= 20U) {
			return;
		}
	}

	spiWrite(kEnable, 0U);
	spiWrite(kBaud, kBaudFast);
	spiWrite(kEnable, 1U);
}

// Only 6/12 MHz are allowed; GPIO chip-select must be high (enforced by
// the shell caller), and every outstanding frame must be drained.
bool SdSpiController::setClockMHz(std::uint32_t mhz) {
	std::uint32_t const divider = mhz == 6U ? kBaudFast : mhz == 12U ? kBaud12MHz
	                                                                 : 0U;
	if (!_initialized || divider == 0U || (spiRead(kStatus) & 1U) != 0U || spiRead(kTxFifoLevel) != 0U || spiRead(kRxFifoLevel) != 0U) {
		return false;
	}

	std::uint32_t const previous = spiRead(kBaud);
	if (previous == divider) {
		return true;
	}

	// BAUDR is writable only when SSIENR=0. No FIFO state is discarded,
	// because all FIFO levels and BUSY were checked above.
	spiWrite(kEnable, 0U);
	spiWrite(kBaud, divider);
	spiWrite(kEnable, 1U);
	if (spiRead(kBaud) != divider) {
		// Best-effort restore to the known rate; report failure to caller.
		spiWrite(kEnable, 0U);
		spiWrite(kBaud, previous);
		spiWrite(kEnable, 1U);
		return false;
	}

	return true;
}

std::uint32_t SdSpiController::clockMHz() const {
	if (!_initialized) {
		return 0U;
	}

	std::uint32_t const div = spiRead(kBaud);
	return div == kBaudFast ? 6U : div == kBaud12MHz ? 12U
	                                                 : 0U;
}

std::uint32_t SdSpiController::version() const {
	return _initialized ? spiRead(kVersion) : 0U;
}

std::uint32_t SdSpiController::divider() const {
	return _initialized ? spiRead(kBaud) : 0U;
}

PicoSpi1FifoStats SdSpiController::fifoStatistics() const {
	return _fifoStats;
}

PicoSpi1FifoMode SdSpiController::fifoMode() const {
	return _fifoMode;
}

bool SdSpiController::setFifoMode(PicoSpi1FifoMode mode) {
	if (!_initialized || (mode != PicoSpi1FifoMode::Safe && mode != PicoSpi1FifoMode::Fast) || spiRead(kTxFifoLevel) != 0U || spiRead(kRxFifoLevel) != 0U) {
		return false;
	}

	_fifoMode = mode;
	return true;
}

bool SdSpiController::resetFifoStatistics() {
	if (!_initialized || spiRead(kTxFifoLevel) != 0U || spiRead(kRxFifoLevel) != 0U) {
		return false;
	}

	_fifoStats = {};
	return true;
}

} // namespace lexxos::platform::rk3506
