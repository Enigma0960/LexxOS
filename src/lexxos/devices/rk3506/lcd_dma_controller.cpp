// SPI0 TX: PL330 channel 2, request 0, event 2. SD сохраняет каналы/события 0/1.
#include <lexxos/devices/rk3506/lcd_dma_controller.h>

#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>
#include <lexxos/drivers/core/dma/pl330_tx_program.h>
#include <lexxos/platform/rockchip/rk3506/dma_resources.h>
#include <lexxos/platform/rockchip/rk3506/dmac0_controller.h>

#if defined(LEXXOS_RK3506_DMA_SAFE)
	#include <lexxos/platform/rockchip/rk3506/mmu_managed.h>
#endif

extern "C" bool rk3506_lcd_dma_irq_configure(bool enabled);

#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
extern "C" std::uint32_t lcd_dma_test_read(std::uintptr_t);
extern "C" void lcd_dma_test_write(std::uintptr_t, std::uint32_t);
extern "C" std::uint32_t lcd_dma_test_sctlr();
extern "C" bool lcd_dma_test_ddr(std::uintptr_t, std::size_t);
extern "C" void lcd_dma_test_program(std::uint8_t const*, std::size_t);
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
// ARM_CA9 portYIELD_FROM_ISR declares the global ulPortYieldRequired with
// a block-scope extern. Expanding it inside a C++ namespace would refer to
// a different (mangled) symbol instead of the variable defined in port.c.
static void lcdDmaRequestYieldFromIsr(BaseType_t higherPriorityWoken) {
	portYIELD_FROM_ISR(higherPriorityWoken);
}
#endif

namespace lexxos::platform::rk3506 {
namespace {
	constexpr std::uintptr_t kDmac = 0xFF000000U;
	constexpr std::uintptr_t kSpi0 = 0xFF120000U;
	constexpr std::uintptr_t kCru = 0xFF9A0000U;
	constexpr std::uintptr_t kGrfRoute = 0xFF2880A8U;
	constexpr std::uintptr_t kGpio0 = 0xFF940000U;
	constexpr std::uint32_t kLcdCsMask = 1U << 4U;
	constexpr std::uintptr_t kSpiTxData = kSpi0 + 0x400U;
	constexpr std::uint32_t kChannel = dma::lcdChannel;
	constexpr std::uint32_t kEvent = dma::lcdEvents;
	constexpr std::uint32_t kChannelFault = dma::lcdChannels;
#if defined(LEXXOS_RK3506_DMA_SAFE)
	#if defined(LEXXOS_RK3506_LCD_DMA_CHUNK_BYTES)
	constexpr std::uint32_t kChunkBytes = LEXXOS_RK3506_LCD_DMA_CHUNK_BYTES;
	#else
	constexpr std::uint32_t kChunkBytes = 16384U;
	#endif
	static_assert(kChunkBytes >= 4096U && kChunkBytes <= 32768U && kChunkBytes % 4096U == 0U);
	#if defined(LEXXOS_RK3506_LCD_DMA_BURST_UNITS)
	constexpr std::uint32_t kBurstUnits = LEXXOS_RK3506_LCD_DMA_BURST_UNITS;
	#else
	constexpr std::uint32_t kBurstUnits = 1U;
	#endif
	static_assert(kBurstUnits == 1U || kBurstUnits == 2U || kBurstUnits == 4U);
#else
	constexpr std::uint32_t kChunkBytes = 4096U;
#endif
	constexpr std::uint32_t kTimeoutMs = 1000U;

	extern "C" std::uint32_t rk3506_timer_ticks();
	extern "C" std::uint64_t rk3506_timer_counter();

	inline std::uint32_t read(std::uintptr_t addr) {
#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
		return lcd_dma_test_read(addr);
#else
		return *reinterpret_cast<std::uint32_t const volatile*>(addr);
#endif
	}

	inline void writeRegister(std::uintptr_t addr, std::uint32_t value) {
#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
		lcd_dma_test_write(addr, value);
#else
		*reinterpret_cast<std::uint32_t volatile*>(addr) = value;
#endif
	}

	inline void barrier() {
#if !defined(LEXXOS_RK3506_LCD_DMA_TEST)
		asm volatile("dsb sy" ::: "memory");
#endif
	}

	std::uint32_t sctlr() {
#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
		return lcd_dma_test_sctlr();
#else
		std::uint32_t value;
		asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(value));
		return value;
#endif
	}

	bool physicalDdr(std::uintptr_t address, std::size_t bytes) {
#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
		return lcd_dma_test_ddr(address, bytes);
#else
		return address >= 0x02000000U && address < 0x08000000U && bytes <= 0x08000000U - address;
#endif
	}

	bool channelIdle() {
		return (read(kDmac + 0x110U) & 15U) == 0U;
	}

	bool controllerFault() {
		return dma::hasFault(read(kDmac + 0x030U), read(kDmac + 0x034U), kChannelFault);
	}

} // namespace

LcdDmaController::LcdDmaController() = default;

#if defined(LEXXOS_RK3506_DMA_SAFE)
bool LcdDmaController::prepareDmaBuffers() {
	if (_dmaStorage == nullptr) {
		// The program occupies its own NC page; never DMA from WBWA .bss.
		_dmaStorage = static_cast<std::uint8_t*>(memory::allocateDma(kChunkBytes + 4096U));
		if (_dmaStorage == nullptr) {
			return false;
		}
		_stage = _dmaStorage;
		_code = _dmaStorage + kChunkBytes;
	}
	return memory::isDmaBuffer(_stage, kChunkBytes)
	    && memory::isDmaBuffer(_code, drivers::pl330::txProgramCapacity);
}
#endif

bool LcdDmaController::buildCode(std::uint8_t const* source, std::uint32_t size, std::uint32_t unitBytes, std::uint8_t burstUnits) {
#if defined(LEXXOS_RK3506_DMA_SAFE)
	_codeBytes = drivers::pl330::buildTxProgram(std::span<std::uint8_t>(_code, drivers::pl330::txProgramCapacity), {
#else
	_codeBytes = drivers::pl330::buildTxProgram(_code, {
#endif
		.source = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(source)),
		.destination = static_cast<std::uint32_t>(kSpiTxData),
		.bytes = size,
		.unitBytes = static_cast<std::uint8_t>(unitBytes),
		.request = dma::lcdTxRequest,
		.event = dma::lcdEvent,
		.burstUnits = burstUnits,
	});
#if defined(LEXXOS_RK3506_LCD_DMA_TEST)
	if (_codeBytes) {
		lcd_dma_test_program(_code, _codeBytes);
	}
#endif
	return _codeBytes != 0;
}

void LcdDmaController::captureFailure(char const* stage) {
	if (_failure.valid) {
		return;
	}

	_failure.valid = true;
	_failure.stage = stage;
	_failure.managerState = read(kDmac + 0x000U);
	_failure.managerPc = read(kDmac + 0x004U);
	_failure.managerFault = read(kDmac + 0x030U);
	_failure.managerFaultType = read(kDmac + 0x038U);
	_failure.channelState = read(kDmac + 0x110U);
	_failure.channelPc = read(kDmac + 0x114U);
	_failure.channelFaults = read(kDmac + 0x034U);
	_failure.channelFaultType = read(kDmac + 0x048U);
	_failure.eventRaw = read(kDmac + 0x024U);
	_failure.eventIrq = read(kDmac + 0x028U);
	_failure.debugStatus = read(kDmac + 0xD00U);
	_failure.spiStatus = read(kSpi0 + 0x024U);
	_failure.spiDmaControl = read(kSpi0 + 0x03CU);
	_failure.spiTxLevel = read(kSpi0 + 0x01CU);
	_failure.codeAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_code));
	_failure.codeBytes = _codeBytes;
}

bool LcdDmaController::fail(char const* reason, bool timeout) {
	captureFailure(reason); // BEFORE DMAKILL: fault registers may change.

	_reason = reason;
	++_stats.errors;
	if (timeout) {
		++_stats.timeouts;
	}

	// Abort ONLY channel 2. Do not reset DMAC0 or modify SPI1.
	(void) dmac0Abort(Dmac0Client::Lcd);
	writeRegister(kSpi0 + 0x03CU, 0U); // Disable SPI0 DMA request
	writeRegister(kSpi0 + 0x0CU, 0U); // SPI0 SER
	writeRegister(kSpi0 + 0x008U, 0U); // Flush SPI0 FIFO
	barrier();
	_enabled = false;
	_probed = false;
	_failed = true;
	// A bool synchronous write may borrow stack or framebuffer memory.
	// Do not return it (or release SPI ownership) until DMA can no longer read.
	_quarantined = !channelIdle();
	if (_quarantined && !_sourceBorrowed) {
		// Persistent controller-owned scratch/code remain quarantined. In
		// particular boot preflight can fail without holding up UART startup.
		return false;
	}

	while (!channelIdle()) {
		if (_wait) {
			_wait(1);
		}
	}
	barrier();
	_quarantined = false;
	_active = false;
	return false;
}

bool LcdDmaController::transmitChunk(std::uint8_t const* data, std::uint32_t size,
    bool registered, std::uint32_t unitBytes) {
	if (_active) {
		return false;
	}

	_active = true;
	_sourceBorrowed = false;

	if (!channelIdle() || controllerFault() || read(kSpi0 + 0x01CU) != 0U || (read(kSpi0 + 0x024U) & 1U) != 0U) {
		return fail("SPI0 or PL330 channel 2 not idle");
	}

	// Synchronous PL330 + SPI FIFO drain. In the managed build, *every*
	// ordinary DDR source goes through Normal-NC staging, even when it is
	// registered. Never DMA directly from the cached LCD framebuffer.
	auto const* source = data;
#if defined(LEXXOS_RK3506_DMA_SAFE)
	(void) registered;
	if (size > kChunkBytes || _stage == nullptr || _code == nullptr) {
		return fail("invalid DMA staging buffer/size");
	}

	// The CPU owns this source; PL330 sees only the NC staging page(s).
	// Measure staging separately from the synchronous PL330/FIFO wait.
	auto const stageStart = rk3506_timer_counter();
	for (std::uint32_t i = 0; i < size; ++i) {
		_stage[i] = data[i];
	}

	_stats.stagingTicks += rk3506_timer_counter() - stageStart;
	_stats.stagingBytes += size;

	source = _stage;
#else
	if (!registered && !physicalDdr(reinterpret_cast<std::uintptr_t>(data), size)) {
		for (std::uint32_t i = 0; i < size; ++i) {
			_stage[i] = data[i];
		}
		source = _stage;
	}
#endif
	_sourceBorrowed = source != _stage;

	// Burst only in RGB565 mode; odd-width/partial rectangles remain valid.
	std::uint8_t burstUnits = 1U;
#if defined(LEXXOS_RK3506_DMA_SAFE)
	if (unitBytes == 2U && size % (unitBytes * kBurstUnits) == 0U) {
		burstUnits = static_cast<std::uint8_t>(kBurstUnits);
	}
#endif
	if (!buildCode(source, size, unitBytes, burstUnits)) {
		return fail("invalid PL330 TX program");
	}

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	// Drop a late signal from the preceding chunk before enabling event 2.
	(void) xSemaphoreTake(_completionSemaphore, 0U);
#endif

	_irqDone = 0U;
	_irqFault = 0U;
	_irqCycles = 0U;
	dmac0Acknowledge(Dmac0Client::Lcd);
	dmac0SetEventsEnabled(Dmac0Client::Lcd, true);
	barrier();

	writeRegister(kSpi0 + 0x040U, 8U); // SPI0 DMATDLR
	writeRegister(kSpi0 + 0x03CU, 2U); // SPI0 TX DMA enable only
	barrier();

	auto const launchCycles = rk3506_timer_counter();
	if (!dmac0StartNonsecure(Dmac0Client::Lcd, kChannel,
	        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_code)))) {
		return fail("PL330 DMAGO channel 2 failed");
	}

	auto const started = rk3506_timer_ticks();
	auto const startCycles = rk3506_timer_counter();

	// IRQ148 executes SEV for the optional WFE mode; ISR semaphore mode
	// instead blocks this task in FreeRTOS until IRQ148 arrives.
	// A one-tick timeout preserves the PL330 fault/timeout watchdog.
	auto waitForCompletion = [this]() {
#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
		++_stats.completionSemaphoreWaits;
		if (xSemaphoreTake(_completionSemaphore, pdMS_TO_TICKS(1)) == pdTRUE) {
			++_stats.completionSemaphoreWakeups;
		} else {
			++_stats.completionSemaphoreTimeouts;
		}
#elif defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_WAIT_WFE) && !defined(LEXXOS_RK3506_LCD_DMA_TEST)
		auto const start = rk3506_timer_counter();
		++_stats.completionWfeWaits;
		asm volatile("dsb sy; wfe" ::: "memory");
		_stats.completionWfeTicks += rk3506_timer_counter() - start;
#else
		if (_wait) {
			++_stats.completionTaskSleeps;
			_wait(1U);
		}
#endif
	};
	while (_irqDone == 0U) {
		if ((read(kDmac + 0x028U) & kEvent) != 0U) {
			++_stats.irqPollFallbacks;
			break;
		}

		if (controllerFault() || _irqFault != 0U) {
			return fail((read(kDmac + 0x030U) & 1U) != 0U
			        ? "PL330 manager fault"
			        : "PL330 channel 2 fault");
		}

		if (std::uint32_t(rk3506_timer_ticks() - started) >= kTimeoutMs) {
			return fail("PL330 channel 2 completion timeout", true);
		}

		waitForCompletion();
	}

	barrier();
	auto const observedCycles = rk3506_timer_counter();
#if defined(LEXXOS_RK3506_DMA_SAFE)
	// IRQ may fire before the final SPI FIFO words leave the bus.
	// Give this normally sub-200 us tail a bounded polling window instead
	// of imposing another entire FreeRTOS scheduler tick.
	#if defined(LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US)
	constexpr std::uint64_t drainSpinTicks = std::uint64_t(LEXXOS_RK3506_LCD_DMA_DRAIN_SPIN_US) * 24U;
	#else
	constexpr std::uint64_t drainSpinTicks = 0U;
	#endif
#endif

	while (!channelIdle() || (read(kSpi0 + 0x024U) & 1U) != 0U || read(kSpi0 + 0x01CU) != 0U) {
		if (controllerFault() || _irqFault != 0U) {
			return fail((read(kDmac + 0x030U) & 1U) != 0U
			        ? "PL330 manager fault after completion"
			        : "PL330 channel 2 fault after completion");
		}

		if (std::uint32_t(rk3506_timer_ticks() - started) >= kTimeoutMs) {
			return fail("SPI0 drain timeout after DMA", true);
		}

#if defined(LEXXOS_RK3506_DMA_SAFE)
		if (rk3506_timer_counter() - observedCycles < drainSpinTicks) {
			++_stats.drainSpinChecks;
			continue;
		}
#endif
		if (_wait) {
			++_stats.drainTaskSleeps;
			_wait(1U);
		}
	}

	if (controllerFault() || _irqFault != 0U) {
		return fail("PL330 fault at completion");
	}

	auto const drainedCycles = rk3506_timer_counter();
	if (_irqDone != 0U && _irqCycles >= launchCycles && _irqCycles <= observedCycles) {
		_stats.irqTicks += _irqCycles - launchCycles;
		_stats.wakeupTicks += observedCycles - _irqCycles;
		++_stats.timedIrqs;
	}
	_stats.drainTicks += drainedCycles - observedCycles;

	dmac0SetEventsEnabled(Dmac0Client::Lcd, false);
	dmac0Acknowledge(Dmac0Client::Lcd);
	writeRegister(kSpi0 + 0x03CU, 0U); // Back to PIO for commands/small writes
	barrier();
	++_stats.chunks;
	if (burstUnits > 1U) {
		++_stats.burstChunks;
		_stats.burstBytes += size;
	} else {
		++_stats.singleChunks;
	}

	if (unitBytes == 2U) {
		++_stats.direct16Chunks;
	}

	_stats.bytes += size;
	_stats.waitTicks += rk3506_timer_counter() - startCycles;
	_active = false;
	return true;
}

bool LcdDmaController::handleIrq(std::uint32_t id) {
	if (!_bound) {
		return false;
	}
	if (id == 149U) {
		if (controllerFault()) {
			_irqFault = 1U;
		}
		return false;
	}

	if (id != 148U || (read(kDmac + 0x028U) & kEvent) == 0U) {
		return false;
	}

	dmac0Acknowledge(Dmac0Client::Lcd);

	_irqCycles = rk3506_timer_counter();
	barrier();

	_irqDone = 1U;
	++_stats.irqInterrupts;
	barrier();

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	BaseType_t higherPriorityWoken = pdFALSE;
	if (_completionSemaphore != nullptr
	    && xSemaphoreGiveFromISR(_completionSemaphore, &higherPriorityWoken) == pdPASS) {
		++_stats.completionSemaphoreSignals;
	}
	// ARM_CA9 port defers the actual context switch until IRQ return.
	lcdDmaRequestYieldFromIsr(higherPriorityWoken);
#endif

#if !defined(LEXXOS_RK3506_LCD_DMA_TEST)
	asm volatile("sev" ::: "memory");
#endif
	return true;
}

void LcdDmaController::setWaitFunction(void (*wait)(std::uint32_t)) {
	if (!_active) {
		_wait = wait;
	}
}

LcdDmaFailure LcdDmaController::lastFailure() {
	return _failure;
}

void LcdDmaController::releaseOwnedSourceIfStopped() {
	if (_quarantined && !_sourceBorrowed && channelIdle()) {
		barrier();
		_quarantined = false;
		_active = false;
	}
}

bool LcdDmaController::probe() {
#if defined(LEXXOS_RK3506_FREERTOS) && !defined(LEXXOS_RK3506_DMA_SAFE)
	_reason = "LCD DMA unavailable with FreeRTOS; use PIO";
	return false;
#endif
	releaseOwnedSourceIfStopped();
	if (!_bound || _active) {
		return false;
	}
	if ((_probed || _enabled || _failed) && !channelIdle()) {
		_reason = "LCD DMA is active";
		return false;
	}

	if (_probed && !_failed) {
		return true;
	}

	_probed = false;
	_enabled = false;
	_failed = false;
	_stats = {};
	_failure = {};
	_codeBytes = 0U;

#if defined(LEXXOS_RK3506_DMA_SAFE)
	if ((sctlr() & 5U) != 5U || !memory::snapshot().active || !prepareDmaBuffers()) {
		_reason = "managed MMU / non-cacheable LCD DMA buffers unavailable";
		return false;
	}

	#if defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	if (_completionSemaphore == nullptr) {
		_completionSemaphore = xSemaphoreCreateBinary();
		if (_completionSemaphore == nullptr) {
			_reason = "LCD DMA FreeRTOS semaphore allocation failed";
			return false;
		}
	}
	#endif

#else
	if ((sctlr() & 5U) != 0U) {
		_reason = "MMU or D-cache enabled; physical DDR DMA is unsafe";
		return false;
	}

	if (!physicalDdr(reinterpret_cast<std::uintptr_t>(_stage), sizeof(_stage))
	    || !physicalDdr(reinterpret_cast<std::uintptr_t>(_code), sizeof(_code))) {
		_reason = "DMA buffers are not in physical Lyra DDR";
		return false;
	}
#endif

	// Same DMAC0 clock gate used by SPI1, enable only; never reset DMAC0.
	dmac0EnableClock();
	barrier();
	if ((read(kDmac + 0xFE0U) & 255U) != 0x30U
	    || (read(kDmac + 0xFE4U) & 255U) != 0x13U
	    || (read(kDmac + 0xFE8U) & 15U) != 0x04U) {
		_reason = "DMAC0 PL330 identification mismatch";
		return false;
	}

	auto const cr0 = read(kDmac + 0xE00U);
	if ((cr0 & 5U) != 5U || ((cr0 >> 4U) & 7U) < kChannel || ((cr0 >> 12U) & 31U) == 0U || (read(kDmac + 0xE10U) & 1U) == 0U || ((cr0 >> 17U) & 31U) < 2U || (read(kDmac + 0xE0CU) & kEvent) == 0U) {
		_reason = "DMAC0 channel 2 / SPI0 request 0 not non-secure";
		return false;
	}

	if (!channelIdle() || controllerFault() || (read(kDmac + 0x020U) & (1U << 2U)) != 0U) {
		_reason = "DMAC0 channel 2 is already owned or faulted";
		return false;
	}

	auto const route = dmac0ConfigureSpiRoute(Dmac0Client::Lcd);
	if (route != Dmac0RouteResult::Ready) {
		_reason = route == Dmac0RouteResult::Conflict
		    ? "DMAC0 request 0 routed to another peripheral"
		    : "SPI0 DMA request routing failed";
		return false;
	}

	// Important: probe the *complete* 4 KiB PL330 microprogram while the
	// LCD GPIO CS is high.  No data reaches the ILI9488 in this state.
	// A failure must not disable the console until the user reboots.
	if ((read(kGpio0 + 0x000U) & kLcdCsMask) == 0U) {
		_reason = "LCD CS is asserted during DMA preflight";
		return false;
	}

	auto const clockHz = lcdSpi0ClockHz();
	if (clockHz == 0U) {
		_reason = "SPI0 PIO is not initialized";
		return false;
	}

	if (!rk3506_lcd_dma_irq_configure(true)) {
		_reason = "LCD DMA IRQ148 registration failed";
		return false;
	}

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	// GICD_IPRIORITYR148 (byte 0) must meet the ARM_CA9 FromISR threshold.
	auto const irqPriority = read(0xFF581000U + 0x400U + 148U) & 0xFFU;
	if (irqPriority < (configMAX_API_CALL_INTERRUPT_PRIORITY << 4U)) {
		(void) rk3506_lcd_dma_irq_configure(false);
		_reason = "IRQ148 priority is too high for FreeRTOS FromISR";
		return false;
	}
#endif

	for (std::uint32_t i = 0; i < kChunkBytes; ++i) {
		_stage[i] = 0x00U;
	}

	if (!transmitChunk(_stage, kChunkBytes)) {
		(void) rk3506_lcd_dma_irq_configure(false);
		// DMA failure stops the SPI0 peripheral.  Re-enable the already
		// configured SPI0 PIO bus before returning to the caller.
		// Never reset DMAC0: SPI1 SD may be using channels 0 and 1.
		if (!lcdSpi0Initialize(clockHz)) {
			_reason = "LCD DMA preflight failed; SPI0 PIO recovery failed";
		}

		return false;
	}

	// The preflight is not part of the pixel throughput measurement.
	_stats = {};
	_reason = "ready (IRQ148 event2, 4 KiB preflight passed)";
	_probed = true;
	return true;
}

bool LcdDmaController::setEnabled(bool enabled) {
	if (!_bound || _active) {
		return false;
	}
	if (!enabled) {
		if (((_probed || _enabled || _failed) && !channelIdle()) || !rk3506_lcd_dma_irq_configure(false)) {
			return false;
		}

		_enabled = false;
		return true;
	}

	if (!_probed || _failed || !channelIdle() || controllerFault()) {
		return false;
	}

	if (!rk3506_lcd_dma_irq_configure(true)) {
		return false;
	}

	_enabled = true;
	return true;
}

bool LcdDmaController::prepareRamReload() {
	releaseOwnedSourceIfStopped();
	if (!_bound || _active) {
		return false;
	}
	if ((_probed || _enabled || _failed) && !channelIdle()) {
		return false;
	}

	return setEnabled(false);
}

bool LcdDmaController::enabled() {
	return _enabled;
}

bool LcdDmaController::registerSource(std::uint8_t const* source, std::size_t capacity) {
	if (!_bound || _active) {
		return false;
	}

#if defined(LEXXOS_RK3506_DMA_SAFE)
	// The registered source is CPU-owned/cacheable and is staged per chunk.
	if (!source || !capacity || capacity > lcdDmaMaxAreaBytes || (sctlr() & 5U) != 5U) {
		return false;
	}
#else
	if (!source || !capacity || capacity > lcdDmaMaxAreaBytes || (sctlr() & 5U) != 0U || !physicalDdr(reinterpret_cast<std::uintptr_t>(source), capacity)) {
		return false;
	}
#endif

	// Источник нельзя подменять, пока канал может читать прежнюю память.
	if (_areaSource) {
		return source == _areaSource && capacity == _areaCapacity;
	}

	_areaSource = source;
	_areaCapacity = capacity;
	return true;
}

bool LcdDmaController::write(std::uint8_t const* source, std::size_t count) {
#if defined(LEXXOS_RK3506_DMA_SAFE)
	if (!_bound || _active || !_enabled || !_probed || _failed || source == nullptr || count == 0U || (sctlr() & 5U) != 5U) {
		return false;
	}
	if (source == _areaSource && count > _areaCapacity) {
		return false;
	}
#else
	if (!_bound || _active || !_enabled || !_probed || _failed || source == nullptr || count == 0U || (sctlr() & 5U) != 0U) {
		return false;
	}

	if (source == _areaSource) {
		if (count > _areaCapacity) {
			return false;
		}
		return transmitChunk(source, static_cast<std::uint32_t>(count), true);
	}
#endif

	while (count != 0U) {
		auto const bytes = count < kChunkBytes ? static_cast<std::uint32_t>(count) : kChunkBytes;
		if (!transmitChunk(source, bytes)) {
			return false; // Never retry PIO after partial DMA output.
		}

		source += bytes;
		count -= bytes;
	}

	return true;
}

bool LcdDmaController::write16(std::uint16_t const* source, std::size_t pixels) {
#if defined(LEXXOS_RK3506_DMA_SAFE)
	if (!_bound || _active || !_enabled || !_probed || _failed || !source || pixels == 0U || pixels > 320U * 320U || (sctlr() & 5U) != 5U) {
		return false;
	}
	// RGB565 sources are cacheable; send even-size Normal-NC chunks.
	auto const* bytes = reinterpret_cast<std::uint8_t const*>(source);
	auto remaining = pixels * sizeof(std::uint16_t);
	while (remaining != 0U) {
		auto const count = remaining > kChunkBytes ? kChunkBytes : static_cast<std::uint32_t>(remaining);
		if (!transmitChunk(bytes, count, false, 2U)) {
			return false; // Partial frame cannot be replayed with PIO.
		}
		bytes += count;
		remaining -= count;
	}
	return true;
#else
	if (!_bound || _active || !_enabled || !_probed || _failed || !source || pixels == 0U || pixels > 320U * 320U || (sctlr() & 5U) != 0U) {
		return false;
	}

	auto const bytes = pixels * sizeof(std::uint16_t);
	if ((reinterpret_cast<std::uintptr_t>(source) & 1U) != 0U
	    || !physicalDdr(reinterpret_cast<std::uintptr_t>(source), bytes)) {
		return false;
	}

	// Never stage/copy or split the source. Caller owns it until IRQ + FIFO drain.
	return transmitChunk(reinterpret_cast<std::uint8_t const*>(source),
	    static_cast<std::uint32_t>(bytes), true, 2U);
#endif
}

char const* LcdDmaController::reason() {
	return _reason;
}

LcdDmaStats LcdDmaController::statistics() {
	return _stats;
}

LcdDmaRegisters LcdDmaController::registerSnapshot() {
	return {
	    .spiClockSelect = read(kCru + 0x388U),
	    .spiBaudDivider = read(kSpi0 + 0x010U),
	    .dmaChannel2State = read(kDmac + 0x110U),
	    .dmaChannelFaults = read(kDmac + 0x034U),
	    .grfDmaRoute = read(kGrfRoute),
	};
}

} // namespace lexxos::platform::rk3506
