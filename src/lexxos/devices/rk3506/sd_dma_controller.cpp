/* Experimental read-only SPI1 DMA using RK3506 DMAC0 (ARM PL330).
 * With a managed MMU, all PL330 memory is in the Normal-NC DMA pool;
 * legacy bare-metal operation still requires MMU and D-cache OFF.
 * SPI1 (0xff130000) is the PicoCalc
 * secondary SD slot. This code DOES NOT touch the boot SDMMC controller.
 * Reference: RK3506 TRM 1.1 chapter 8, ARM PL330 microcode,
 * Linux drivers/dma/pl330.c and drivers/spi/spi-rockchip.c.
 * No writes to the card. No implicit fallback after a partial DMA block.
 */
#include <lexxos/devices/rk3506/sd_dma_controller.h>

#include <lexxos/devices/rk3506/picosd_dma_irq_wait.h>
#include <lexxos/platform/rockchip/rk3506/dma_resources.h>
#include <lexxos/platform/rockchip/rk3506/dmac0_controller.h>

#if defined(LEXXOS_RK3506_PMU_PROFILING)
	#include <lexxos/platform/rockchip/rk3506/pmu_profile.h>
#endif

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE)
	#include <lexxos/platform/rockchip/rk3506/mmu_managed.h>
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t kDmac = 0xFF000000U; // DMAC0 non-secure APB
	constexpr std::uintptr_t kSpi = 0xFF130000U; // SPI1 FIFO
	constexpr std::uintptr_t kGrf = 0xFF288000U;
	constexpr std::uintptr_t kSpiRxData = kSpi + 0x800U;
	constexpr std::uintptr_t kSpiTxData = kSpi + 0x400U;
	constexpr std::uint32_t kMaxBytes = 512U;
	constexpr std::uint32_t kTimeoutMs = 500U;
	constexpr std::uint32_t kEvents = dma::sdEvents; // PL330 event 0 RX, event 1 TX.
	constexpr std::uint32_t kCcrNonsecure = (1U << 9) | (1U << 23);
	extern "C" std::uint32_t rk3506_timer_ticks();
	extern "C" std::uint64_t rk3506_timer_counter();

	// Cortex-A7 ARMv7-A CNTKCTL (Non-secure PL1, CP15 c14/c1/0).
	// Bits 7:4 EVNTI, bit 3 EVNTDIR, bit 2 EVNTEN.
	// CNTVCT is 24 MHz here; bit 9 rising edge generates a local WFE
	// event every 1024 ticks (~42.7 us). No GIC interrupt is involved.
	constexpr std::uint32_t kEventEnable = 1U << 2U;
	constexpr std::uint32_t kEventMask = (0xFU << 4U) | (1U << 3U) | kEventEnable;
	constexpr std::uint32_t kEventConfiguration = (9U << 4U) | kEventEnable;

	std::uint32_t readCntkctl() {
		std::uint32_t value;
		asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r"(value));
		return value;
	}

	void writeCntkctl(std::uint32_t value) {
		asm volatile("mcr p15, 0, %0, c14, c1, 0; isb" : : "r"(value) : "memory");
	}

	inline std::uint32_t read(std::uintptr_t addr) {
		return *reinterpret_cast<std::uint32_t const volatile*>(addr);
	}

	inline void write(std::uintptr_t addr, std::uint32_t value) {
		*reinterpret_cast<std::uint32_t volatile*>(addr) = value;
	}

	inline void sync() {
		asm volatile("dsb sy" ::: "memory");
	}

	std::uint32_t sctlr() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(v));
		return v;
	}

	// PL330 exposes a PrimeCell ID as four byte registers.
	bool primecellIdOk() {
		return (read(kDmac + 0xFE0U) & 255U) == 0x30U
		    && (read(kDmac + 0xFE4U) & 255U) == 0x13U
		    && (read(kDmac + 0xFE8U) & 0x0FU) == 0x04U;
	}

	bool idle() {
		return (read(kDmac + 0x000U) & 15U) == 0U // manager stopped
		    && (read(kDmac + 0x100U) & 15U) == 0U // channel 0 stopped
		    && (read(kDmac + 0x108U) & 15U) == 0U // channel 1 stopped
		    && (read(kDmac + 0x034U) & 3U) == 0U; // channel faults
	}

	struct Code {
		std::uint8_t* ptr;
		std::uint32_t used{};

		void emit(std::uint8_t b) {
			ptr[used++] = b;
		}

		std::uint32_t mov(std::uint8_t reg, std::uint32_t value) {
			emit(0xBCU);
			emit(reg);
			auto const operand = used;

			for (unsigned i = 0; i < 4; ++i) {
				emit(std::uint8_t(value >> (i * 8U)));
			}

			return operand;
		}

		void loop(std::uint8_t peripheral, bool isRx, std::uint32_t size) {
			// Rockchip peripherals normally signal BURST requests even when the
			// CCR burst length is one. The PL330 Rockchip quirk
			// "arm,pl330-periph-burst" forces BURST opcodes in this case;
			// SINGLE opcodes can remain blocked in WFP forever. Keep one
			// byte per transfer to avoid overwhelming SPI RX FIFO.
			// PL330 LPEND jumps backwards from the LPEND address, not end.
			for (std::uint32_t outer = 0; outer < size; outer += 256U) {
				auto const chunk = size - outer > 256U ? 256U : size - outer;

				emit(0x20U);
				emit(std::uint8_t(chunk - 1U)); // DMALP_0
				std::uint32_t const body = used;
				emit(0x32U);
				emit(std::uint8_t(peripheral << 3U)); // DMAWFP B

				if (isRx) {
					emit(0x27U);
					emit(std::uint8_t(peripheral << 3U)); // DMALDP B
					emit(0x0BU); // DMAST B
				} else {
					emit(0x07U); // DMALD B
					emit(0x2BU);
					emit(std::uint8_t(peripheral << 3U)); // DMASTP B
				}

				auto const backward = used - body;

				emit(0x38U);
				emit(std::uint8_t(backward)); // DMALPEND_0
			}
		}
	};

	static_assert(kMaxBytes == 512U);

	bool startChannel(unsigned channel, std::uint8_t const* code) {
		// DMAGO with NS bit set; launch by manager debug interface.
		return dmac0StartNonsecure(Dmac0Client::Sd, channel, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code)));
	}

} // namespace

extern "C" bool rk3506_dma_irq_configure(bool enabled);

SdDmaController::SdDmaController() = default;

#if defined(LEXXOS_RK3506_DMA_SAFE)
bool SdDmaController::prepareDmaBuffers() {
	if (_dmaStorage == nullptr) {
		// A single 4 KiB allocation keeps TX/RX and both 64-byte programs
		// physically contiguous and aligned; the remaining bytes stay unused.
		_dmaStorage = static_cast<std::uint8_t*>(memory::allocateDma(4096U));
		if (_dmaStorage == nullptr) {
			return false;
		}

		_tx = _dmaStorage;
		_rx = _dmaStorage + 512U;
		_rxCode = _dmaStorage + 1024U;
		_txCode = _dmaStorage + 1088U;
	}

	return memory::isDmaBuffer(_tx, 512U)
	    && memory::isDmaBuffer(_rx, 512U)
	    && memory::isDmaBuffer(_rxCode, 64U)
	    && memory::isDmaBuffer(_txCode, 64U);
}
#endif

void SdDmaController::stopEventWait() {
	if (!_waitEvent) {
		return;
	}

	// Restore only the event-stream fields, preserving unrelated timer
	// controls. We modify these fields only while our mode is selected.
	auto const current = readCntkctl();
	writeCntkctl((current & ~kEventMask) | _savedEventBits);
	_waitEvent = false;
}

void SdDmaController::captureFailure(char const* stage) {
	if (_failure.valid) {
		return;
	}

	_failure.valid = true;
	_failure.stage = stage;
	_failure.manager = read(kDmac + 0x000U);
	_failure.rxChannel = read(kDmac + 0x100U);
	_failure.txChannel = read(kDmac + 0x108U);
	_failure.events = read(kDmac + 0x028U);
	_failure.managerFault = read(kDmac + 0x030U);
	_failure.channelFaults = read(kDmac + 0x034U);
	_failure.rxFaultType = read(kDmac + 0x040U);
	_failure.txFaultType = read(kDmac + 0x044U);
	_failure.rxPc = read(kDmac + 0x104U);
	_failure.txPc = read(kDmac + 0x10CU);
	_failure.spiStatus = read(kSpi + 0x024U);
	_failure.spiIrq = read(kSpi + 0x034U);
	_failure.spiTxLevel = read(kSpi + 0x01CU);
	_failure.spiRxLevel = read(kSpi + 0x020U);
	_failure.spiDmaControl = read(kSpi + 0x03CU);
	_failure.spiTxThreshold = read(kSpi + 0x040U);
	_failure.spiRxThreshold = read(kSpi + 0x044U);
	_failure.spiEnabled = read(kSpi + 0x008U);
	_failure.spiSlaveEnabled = read(kSpi + 0x00CU);
	_failure.grfSocCon42 = read(kGrf + 0x0A8U);
	_failure.rxCodeAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_rxCode));
	_failure.txCodeAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_txCode));
}

void SdDmaController::buildCode() {
	// PL330 DMAMOV CCR: 1 byte/burst, non-secure, no cache allocations;
	// memory-side increment only, SPI FIFO aperture address is fixed.
	Code rx{_rxCode};
	rx.mov(0U, std::uint32_t(kSpiRxData)); // SAR

	_rxCurrentDestination = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_rx));
	_rxDarOffset = rx.mov(2U, _rxCurrentDestination); // DAR

	rx.mov(1U, kCcrNonsecure | (1U << 14)); // CCR: dst inc
	rx.emit(0x35U);
	rx.emit(3U << 3U); // DMAFLUSHP SPI1_RX
	rx.loop(3U, true, kMaxBytes);
	rx.emit(0x13U); // DMAWMB: drain outstanding DDR writes
	rx.emit(0x34U);
	rx.emit(0U); // SEV event 0
	rx.emit(0x00U); // END
	Code tx{_txCode};
	tx.mov(0U, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_tx))); // SAR
	tx.mov(2U, std::uint32_t(kSpiTxData)); // DAR
	tx.mov(1U, kCcrNonsecure | 1U); // CCR: src inc
	tx.emit(0x35U);
	tx.emit(2U << 3U); // DMAFLUSHP SPI1_TX
	tx.loop(2U, false, kMaxBytes);
	tx.emit(0x13U); // DMAWMB
	tx.emit(0x34U);
	tx.emit(1U << 3U); // SEV event 1
	tx.emit(0x00U); // END
}

void SdDmaController::kill() {
	// A timed-out transfer cannot be resumed. Stop DMA and SPI, then
	// mark transport failed; caller aborts the SD command and reprobes.
	(void) dmac0Abort(Dmac0Client::Sd);
	write(kSpi + 0x3CU, 0U); // DMACR
	write(kSpi + 0x0CU, 0U); // SER
	write(kSpi + 0x08U, 0U); // SSIENR, flush both FIFOs
	sync();
	_enabled = false;
	_probed = false;
	_fault = true;
}

// Called by GIC after interrupt ACK and before GIC EOI. Never log or
// perform DMA channel restart from IRQ context. false => mask the line
// before EOI to prevent a level-triggered abort/spurious IRQ storm.
bool SdDmaController::handleIrq(std::uint32_t id) {
	if (id == 149U) { // Second DMAC0 output, GIC_SPI 117 + 32 (unclassified).
		if (dma::hasFault(read(kDmac + 0x030U), read(kDmac + 0x034U), dma::sdChannels)) {
			_irqFault = 1U;
			asm volatile("sev" ::: "memory");
		}

		return false;
	}

	if (id != 148U) { // DMAC0 event: GIC_SPI 116 + 32 (board DTB).
		return false;
	}

	std::uint32_t const events = read(kDmac + 0x028U) & kEvents;
	if (events != 0U) {
		dmac0Acknowledge(Dmac0Client::Sd, events); // Clear PL330 level source BEFORE GIC EOI.
		_irqEvents = _irqEvents | events;
		++_stats.irqInterrupts;

		if ((events & 1U) != 0U) {
			++_stats.irqRxEvents;
		}

		if ((events & 2U) != 0U) {
			++_stats.irqTxEvents;
		}

		if (events == kEvents) {
			++_stats.irqCombinedEvents;
		}

		asm volatile("sev" ::: "memory");
		return true;
	}

	// На общей IRQ148 причиной может быть событие LCD; чужие биты не трогаем.
	return false;
}

void SdDmaController::reset() {
	stopEventWait();

	if (_waitIrq) {
		rk3506_dma_irq_configure(false);
	}

	_waitIrq = false;
	_irqEvents = 0U;
	_irqFault = 0U;
	_enabled = false;
	_probed = false;
	_fault = false;
	_directTarget = false;

	_reason = "run picosd dma probe";

	_stats = {};
	_failure = {};
	_missedIrq = {};
}

bool SdDmaController::probe() {
	if (!_bound) {
		return false;
	}

#if defined(LEXXOS_RK3506_FREERTOS) && !defined(LEXXOS_RK3506_DMA_SAFE)
	_reason = "SD DMA unavailable with FreeRTOS; use PIO";
	return false;
#endif

	stopEventWait();

	if (_waitIrq) {
		rk3506_dma_irq_configure(false);
	}

	_waitIrq = false;
	_irqEvents = 0U;
	_irqFault = 0U;
	_probed = false;
	_enabled = false;
	_directTarget = false; // Explicit opt-in is required after each reprobe.

#if defined(LEXXOS_RK3506_DMA_SAFE)
	if ((sctlr() & 5U) != 5U || !memory::snapshot().active || !prepareDmaBuffers()) {
		_reason = "managed MMU / non-cacheable SD DMA buffers unavailable";
		return false;
	}
#else
	if ((sctlr() & 5U) != 0U) {
		_reason = "MMU or D-cache enabled (DMA needs uncached physical DDR)";
		return false;
	}
#endif

	// ACLK_DMAC0, CRU CLKGATE_CON5 bit 8; only touch this bit.
	dmac0EnableClock();
	sync();

	if (!primecellIdOk()) {
		_reason = "DMAC0 PL330 identification mismatch";
		return false;
	}

	auto const cr0 = read(kDmac + 0xE00U);
	if ((cr0 & 1U) == 0U || (cr0 & 4U) == 0U || ((cr0 >> 4U) & 7U) < 1U || ((cr0 >> 12U) & 31U) < 3U || ((cr0 >> 17U) & 31U) < 1U || (read(kDmac + 0xE10U) & ((1U << 2U) | (1U << 3U))) != ((1U << 2U) | (1U << 3U)) || (read(kDmac + 0xE0CU) & kEvents) != kEvents) {
		_reason = "DMAC0 channels/requests/events not non-secure";
		return false;
	}

	if (!idle() || (read(kDmac + 0x020U) & kEvents) != 0U) {
		_reason = "DMAC0 channels/events already in use";
		return false;
	}

	auto const route = dmac0ConfigureSpiRoute(Dmac0Client::Sd);
	if (route != Dmac0RouteResult::Ready) {
		_reason = route == Dmac0RouteResult::Conflict
		    ? "DMAC0 requests 2/3 routed to another peripheral"
		    : "DMAC0 SPI1 GRF route write did not stick";
		return false;
	}

	for (std::size_t i = 0; i < kMaxBytes; ++i) {
		_tx[i] = 0xFFU;
	}

	buildCode();
	sync();
	_reason = "ready (experimental opt-in)";
	_probed = true;
	_fault = false;
	return true;
}

bool SdDmaController::enabled() {
	return _enabled;
}

bool SdDmaController::setEnabled(bool enabled) {
	if (!enabled) {
		_enabled = false;
		return true;
	}

	if (!_probed || _fault || !idle()) {
		return false;
	}

	_enabled = true;
	return true;
}

bool SdDmaController::prepareRamReload() {
	// The cooperative shell invokes this with no SD I/O in progress.
	// Do not overwrite the PL330 microcode/buffers while a channel is active.
	if ((_probed || _enabled || _waitIrq || _waitEvent) && !idle()) {
		return false;
	}

	_enabled = false;
	if (_waitIrq) {
		if (!rk3506_dma_irq_configure(false)) {
			return false;
		}

		_waitIrq = false;
	}

	stopEventWait();
	return true;
}

bool SdDmaController::setDirect(bool enabled) {
	if (!_probed || _fault || !idle()) {
		return false;
	}

	_directTarget = enabled;

	return true;
}

bool SdDmaController::direct() {
	return _directTarget;
}

bool SdDmaController::setWaitIrq(bool enabled) {
	if (!_probed || _fault || !idle()) {
		return false;
	}

	if (!enabled) {
		stopEventWait(); // 'wait poll' returns every experimental mode to polling.
		rk3506_dma_irq_configure(false);
		_waitIrq = false;
		return true;
	}

	if (_waitEvent) {
		return false; // Select poll first; never own GIC IRQ and event stream together.
	}

	// Explicit shell opt-in only. In Non-secure GICv2 the group is Unknown;
	// successful enable/readback does not prove ownership of Secure Group0.
	_irqEvents = 0U;
	_irqFault = 0U;

	if (!rk3506_dma_irq_configure(true)) {
		return false;
	}

	_waitIrq = true;
	return true;
}

bool SdDmaController::waitIrq() {
	return _waitIrq;
}

bool SdDmaController::setWaitEvent(bool enabled) {
	if (!_probed || _fault || !idle()) {
		return false;
	}

	if (!enabled) {
		stopEventWait();
		return true;
	}

	if (_waitIrq) {
		return false;
	}

	if (_waitEvent) {
		return true;
	}

	// Do not hijack an event stream already enabled by other firmware/code.
	auto const previous = readCntkctl();
	if ((previous & kEventEnable) != 0U) {
		return false;
	}

	_savedEventBits = previous & kEventMask;
	writeCntkctl((previous & ~kEventMask) | kEventConfiguration);

	if ((readCntkctl() & kEventMask) != kEventConfiguration) {
		writeCntkctl(previous);
		return false;
	}

	_waitEvent = true;
	return true;
}

bool SdDmaController::waitEvent() {
	return _waitEvent;
}

PicoSpi1DmaInfo SdDmaController::info() {
	PicoSpi1DmaInfo result{};
	result.available = _probed && !_fault;
	result.reason = _reason;
	return result;
}

PicoSdDmaStats SdDmaController::statistics() {
	return _stats;
}

PicoSdDmaFailure SdDmaController::failure() {
	return _failure;
}

PicoSdDmaMissedIrq SdDmaController::missedIrq() {
	return _missedIrq;
}

PicoSdDmaIrqRegisters SdDmaController::irqRegisters() {
	if (!_probed) {
		return {}; // Clock and register access have not been validated.
	}

	return {
	    .available = true,
	    .enabledEvents = read(kDmac + 0x020U), // INTEN
	    .eventStatus = read(kDmac + 0x024U), // ES (raw events)
	    .pendingEvents = read(kDmac + 0x028U), // INTSTATUS
	    .managerFault = read(kDmac + 0x030U), // FSRD
	    .channelFaults = read(kDmac + 0x034U), // FSRC
	    .grfSocCon38 = read(kGrf + 0x098U),
	    .grfSocCon39 = read(kGrf + 0x09CU),
	};
}

bool SdDmaController::readData(std::uint8_t* dest, std::size_t size) {
	if (!_probed || !_enabled || _fault || dest == nullptr || size != kMaxBytes) {
		return false;
	}

	if (!idle() || read(kSpi + 0x1CU) != 0U || read(kSpi + 0x20U) != 0U) {
		++_stats.errors;
		_reason = "DMA/SPI not idle before payload (reprobe required)";
		captureFailure("preflight");
		kill();
		return false;
	}

	// Direct DMA is legal only into an allocated non-cacheable buffer.
	// The FAT cache and normal .lexx buffers are cacheable: copy only after
	// BOTH PL330 channels have stopped and completion has been verified.
	auto const address = reinterpret_cast<std::uintptr_t>(dest);
#if defined(LEXXOS_RK3506_DMA_SAFE)
	bool const direct = _directTarget && (address & 3U) == 0U && memory::isDmaBuffer(dest, kMaxBytes);
#else
	bool const direct = _directTarget && (address & 3U) == 0U && address >= 0x02000000U && address <= (0x08000000U - kMaxBytes);
#endif

	if (_directTarget && !direct) {
		++_stats.directFallbacks;
	}

	std::uint8_t* const target = direct ? dest : _rx;

	// Channel 0 is stopped (preflight above). Change only its DMAMOV DAR
	// immediate; never rebuild or edit a running PL330 program.
	auto const dmaAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));

	if (dmaAddress != _rxCurrentDestination) {
		for (unsigned i = 0; i < 4; ++i) {
			_rxCode[_rxDarOffset + i] = std::uint8_t(dmaAddress >> (8U * i));
		}

		_rxCurrentDestination = dmaAddress;
	}

	auto const started = rk3506_timer_counter();

	// Every successful read overwrites all 512 bytes in _rx; on failure
	// _rx is never copied to the caller. Avoid an unnecessary DDR clear.
	// The SPI controller generates a clock only when TX transmits 0xff.
	// Both channels are idle here; no prior IRQ can write their completion.
	_irqEvents = 0U;
	_irqFault = 0U;

	sync();

	dmac0Acknowledge(Dmac0Client::Sd); // acknowledge stale event (channels idle)
	dmac0SetEventsEnabled(Dmac0Client::Sd, true);
	// DMA RX threshold 0 (>=1 byte), TX threshold 8 (<=8 entries).
	write(kSpi + 0x044U, 0U);
	write(kSpi + 0x040U, 8U);
	write(kSpi + 0x03CU, 3U);
	sync();

	auto const configured = rk3506_timer_counter();
	bool const rxStarted = startChannel(0U, _rxCode);
	bool const txStarted = rxStarted && startChannel(1U, _txCode);
	auto const launched = rk3506_timer_counter();
	bool timeout = false;
	bool fault = !txStarted;
	char const* failureStage = "channel-start";

	if (!fault) {
		auto const start = rk3506_timer_ticks();
		auto const waitStarted = rk3506_timer_counter();

#if defined(LEXXOS_RK3506_PMU_PROFILING)
		auto const waitPmuStart = pmuSample();
#endif

		for (;;) {
			++_stats.waitChecks;

			// ISR clears INTSTATUS. Prefer sticky ISR events in IRQ mode;
			// if delivery is lost, raw PL330 completion is a rescue signal.
			// Даже в poll-режиме общая линия LCD может доставить/очистить события SD.
			std::uint32_t const events = _waitIrq ? _irqEvents : (_irqEvents | read(kDmac + 0x028U));

			if ((events & kEvents) == kEvents) {
				break;
			}

			if (_waitIrq && _irqFault != 0U) {
				failureStage = "DMAC0-IRQ-fault";
				fault = true;
				break;
			}

			if ((read(kDmac + 0x030U) & 1U) != 0U || (read(kDmac + 0x034U) & 3U) != 0U) {
				failureStage = "PL330-fault";
				fault = true;
				break;
			}

			if ((read(kSpi + 0x034U) & (1U << 3U)) != 0U) {
				failureStage = "SPI-RX-overflow";
				fault = true;
				break;
			}

			if (std::uint32_t(rk3506_timer_ticks() - start) > kTimeoutMs) {
				failureStage = "completion-timeout";
				timeout = true;
				break;
			}

			if (_waitIrq) {
				// Both DMASEV events may complete without any GIC148 delivery.
				// Preserve a pre-teardown snapshot and release ONLY our IRQ148
				// subscription before accepting the already-complete transfer.
				// This is not fallback after a partial DMA transfer.
				std::uint32_t const raw = read(kDmac + 0x028U);
				if (detail::dmaEventsCompletedWithoutIrq(events, raw, kEvents)) {
					if (!_missedIrq.valid) {
						_missedIrq.valid = true;
						_missedIrq.pl330Inten = read(kDmac + 0x020U);
						_missedIrq.pl330EventStatus = read(kDmac + 0x024U);
						_missedIrq.pl330Intstatus = raw;
						_missedIrq.grfSocCon38 = read(kGrf + 0x098U);
						_missedIrq.grfSocCon39 = read(kGrf + 0x09CU);
#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
						GicIrqSnapshot snapshot{};
						if (rk3506_gic_irq_snapshot(148U, &snapshot)) {
							_missedIrq.gicValid = true;
							_missedIrq.gicEnabled = snapshot.enabled;
							_missedIrq.gicPending = snapshot.pending;
							_missedIrq.gicActive = snapshot.active;
							_missedIrq.gicTarget = snapshot.target;
							_missedIrq.gicPriority = snapshot.priority;
							_missedIrq.gicPmr = snapshot.cpuPriorityMask;
							_missedIrq.gicHppir = snapshot.cpuHighestPending;
						}

						GicIrqBankSnapshot banks{};
						if (rk3506_gic_irq_bank_snapshot(&banks)) {
							_missedIrq.gicBanksValid = true;
							_missedIrq.gicBankCount = banks.bankCount;

							for (std::uint32_t bank = 0; bank < banks.bankCount; ++bank) {
								_missedIrq.gicEnabledBanks[bank] = banks.enabled[bank];
								_missedIrq.gicPendingBanks[bank] = banks.pending[bank];
								_missedIrq.gicActiveBanks[bank] = banks.active[bank];
							}
						}
#endif
					}

					if (!rk3506_dma_irq_configure(false)) {
						failureStage = "IRQ148-release-failed";
						fault = true;
						break;
					}

					_waitIrq = false;
					++_stats.irqPollFallbacks;
					break; // Normal completion path will clear INTSTATUS and verify idle.
				}

				// The existing 1 ms timer wakes WFE even if GIC148 never arrives.
				++_stats.irqSleeps;

				auto const wfeStarted = rk3506_timer_counter();
				asm volatile("dsb sy; wfe" ::: "memory");
				_stats.irqWfeCycles += rk3506_timer_counter() - wfeStarted;
			} else if (_waitEvent) {
				// Virtual counter event stream wakes WFE approximately every
				// 43 us. Poll PL330 status upon wake; no GIC IRQ is claimed.
				++_stats.eventSleeps;

				auto const wfeStarted = rk3506_timer_counter();
				asm volatile("dsb sy; wfe" ::: "memory");
				_stats.eventWfeCycles += rk3506_timer_counter() - wfeStarted;
			} else {
				++_stats.waitPolls;
			}
		}

		_stats.waitCycles += rk3506_timer_counter() - waitStarted;

#if defined(LEXXOS_RK3506_PMU_PROFILING)
		if (waitPmuStart.valid) {
			auto const waitDelta = pmuDifference(waitPmuStart, pmuSample());
			if (waitDelta.valid) {
				_stats.waitPmuCycles += waitDelta.cycles;
				_stats.waitPmuInstructions += waitDelta.instructions;
				++_stats.waitPmuSamples;
			} else {
				++_stats.waitPmuInvalid;
			}
		}
#endif
	}

	auto const waited = rk3506_timer_counter();

	if (!fault && !timeout) {
		sync();
		// Both PL330 channel programs emitted their completion events.
		dmac0Acknowledge(Dmac0Client::Sd);
		dmac0SetEventsEnabled(Dmac0Client::Sd, false);
		write(kSpi + 0x03CU, 0U);
		sync();
		auto const settleStart = rk3506_timer_ticks();

		while (!idle() && std::uint32_t(rk3506_timer_ticks() - settleStart) < 20U) {
		}

		if (!idle() || read(kSpi + 0x1CU) != 0U || read(kSpi + 0x20U) != 0U) {
			failureStage = "completion-not-idle";
			fault = true;
		}
	}

	if (fault || timeout) {
		++_stats.errors;
		if (timeout) {
			++_stats.timeouts;
		}

		_reason = timeout ? "DMA transfer timeout (reprobe required)" : "DMA channel/SPI fault (reprobe required)";
		captureFailure(failureStage); // Capture before KILL clears channel state.
		kill();

		return false;
	}

	// The safe buffered mode never exposes a partial transfer. Direct mode
	// is opt-in and CAN modify the caller's buffer before a DMA/CRC error.
	auto const copyStarted = rk3506_timer_counter();

	if (!direct) {
		for (std::size_t i = 0; i < size; ++i) {
			dest[i] = _rx[i];
		}
	}

	if (direct) {
		++_stats.directReads;
	} else {
		++_stats.bufferedReads;
	}

	auto const copied = rk3506_timer_counter();
	_stats.setupCycles += configured - started;
	_stats.launchCycles += launched - configured;
	_stats.finishCycles += copyStarted - waited;
	_stats.copyCycles += copied - copyStarted;

	++_stats.reads;

	_stats.bytes += std::uint32_t(size);
	_stats.payloadCycles += copied - started;

	return true;
}

} // namespace lexxos::platform::rk3506
