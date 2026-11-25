#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::platform::rk3506 {

struct LcdDmaStats {
	std::uint32_t chunks{};
	std::uint32_t direct16Chunks{};
	std::uint32_t burstChunks{}; // Successful PL330 transfers with 2/4 beats per request.
	std::uint32_t singleChunks{}; // Legacy path, including unaligned-sized transfers.
	std::uint64_t burstBytes{};
	std::uint64_t bytes{};
	std::uint64_t stagingBytes{}; // Cacheable CPU source copied to NC bounce.
	std::uint64_t stagingTicks{}; // At 24 MHz, excludes PL330 transfer time.
	std::uint64_t waitTicks{}; // 24 MHz RK3506 generic timer ticks.
	std::uint64_t irqTicks{}; // launch -> IRQ timestamp (only hardware IRQs).
	std::uint64_t wakeupTicks{}; // IRQ timestamp -> task observes completion.
	std::uint64_t drainTicks{}; // task observes completion -> FIFO is empty.
	std::uint32_t timedIrqs{};
	std::uint32_t errors{};
	std::uint32_t timeouts{};
	std::uint32_t irqInterrupts{};
	std::uint32_t irqPollFallbacks{};
	// Task-context wait diagnostics; IRQ148 contributes only irqInterrupts.
	std::uint32_t completionWfeWaits{};
	std::uint64_t completionWfeTicks{};
	std::uint32_t completionTaskSleeps{};
	std::uint32_t completionSemaphoreWaits{};
	std::uint32_t completionSemaphoreWakeups{};
	std::uint32_t completionSemaphoreTimeouts{};
	std::uint32_t completionSemaphoreSignals{};
	std::uint32_t drainTaskSleeps{};
	std::uint32_t drainSpinChecks{};
};

// Latched BEFORE DMAKILL.  The ordinary register snapshot is taken after
// teardown and cannot reliably distinguish manager vs channel faults.
struct LcdDmaFailure {
	bool valid{};
	char const* stage{"none"};
	std::uint32_t managerState{}, managerPc{}, managerFault{}, managerFaultType{};
	std::uint32_t channelState{}, channelPc{}, channelFaults{}, channelFaultType{};
	std::uint32_t eventRaw{}, eventIrq{}, debugStatus{};
	std::uint32_t spiStatus{}, spiDmaControl{}, spiTxLevel{};
	std::uint32_t codeAddress{}, codeBytes{};
};

struct LcdDmaRegisters {
	std::uint32_t spiClockSelect{}; // CLKSEL_CON34 raw readback (incl. SPI1 fields).
	std::uint32_t spiBaudDivider{};
	std::uint32_t dmaChannel2State{};
	std::uint32_t dmaChannelFaults{};
	std::uint32_t grfDmaRoute{};
};

class LcdDmaController;
// Boot-only binding; instance and all registered storage must outlive IRQ/runtime.
bool lcdDmaBindController(LcdDmaController& controller);
// True until a synchronous transfer has proved its source is no longer in use.
bool lcdDmaSourceRetained();
bool lcdDmaQuarantined();

// SPI0 TX: канал 2, request 0, event 2. SD сохраняет каналы/события 0/1.
// Ожидание разрешено только из кооперативной задачи, не из IRQ.
void lcdDmaSetWaitFunction(void (*wait)(std::uint32_t));
bool lcdDmaProbe();
bool lcdDmaSetEnabled(bool enabled);
bool lcdDmaEnabled();
// Вызывается с захваченной SPI0; не останавливает активный канал принудительно.
bool lcdDmaPrepareRamReload();
// Один неизменный источник с boot lifetime, регистрация при захваченной SPI0.
// Проверка DDR/MMU/cache выполняется при регистрации, данные не проверяются.
inline constexpr std::size_t lcdDmaMaxAreaBytes = 320U * 320U * 3U;
bool lcdDmaRegisterSource(std::uint8_t const* source, std::size_t capacity);
bool lcdDmaWrite(std::uint8_t const* source, std::size_t count);
// Exactly one DMA transfer of aligned RGB565 halfwords directly from physical DDR.
bool lcdDmaWrite16(std::uint16_t const* source, std::size_t pixels);
char const* lcdDmaReason();
LcdDmaStats lcdDmaStatistics();
LcdDmaFailure lcdDmaLastFailure();
LcdDmaRegisters lcdDmaRegisterSnapshot();

} // namespace lexxos::platform::rk3506
