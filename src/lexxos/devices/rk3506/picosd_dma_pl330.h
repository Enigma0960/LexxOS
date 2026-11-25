#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_spi1_hw.h>

namespace lexxos::platform::rk3506 {

struct PicoSdDmaStats {
	std::uint32_t reads{};
	std::uint32_t bytes{};
	std::uint32_t errors{};
	std::uint32_t timeouts{};
	std::uint32_t directReads{}; // Successful zero-copy sectors.
	std::uint32_t bufferedReads{}; // Successful bounce-buffer sectors.
	std::uint32_t directFallbacks{}; // Direct requested, but destination unsuitable.
	std::uint32_t irqSleeps{}; // WFE entries (experimental IRQ mode).
	std::uint32_t irqInterrupts{}; // GIC deliveries for both event channels.
	std::uint32_t irqRxEvents{}; // ISR observations of PL330 RX event bit 0.
	std::uint32_t irqTxEvents{}; // ISR observations of PL330 TX event bit 1.
	std::uint32_t irqCombinedEvents{}; // Both bits observed by one ISR.
	std::uint32_t irqEmptyInterrupts{}; // IRQ148 arrived without our event bits.
	std::uint32_t irqPollFallbacks{}; // Transfer completed but no corresponding GIC delivery.
	std::uint32_t eventSleeps{}; // Virtual-timer event stream WFE wake attempts.
	std::uint64_t payloadCycles{};
	std::uint64_t waitCycles{}; // Elapsed wait-loop time, includes WFE and IRQ handling.
	std::uint64_t irqWfeCycles{}; // CNTVCT wall-time across IRQ/WFE instructions, including IRQ service.
	std::uint64_t eventWfeCycles{}; // CNTVCT wall-time across event-stream/WFE instructions.
	std::uint32_t waitChecks{}; // All wait-loop checks, including final completion/fault check.
	std::uint32_t waitPolls{}; // Spin iterations only, excluding WFE attempts.
	std::uint64_t waitPmuCycles{}; // PMU cycles in DMA wait; valid samples only.
	std::uint64_t waitPmuInstructions{}; // PMU event 0x08 in DMA wait.
	std::uint32_t waitPmuSamples{};
	std::uint32_t waitPmuInvalid{};
	// Successful 512-byte transfers only: phases of payloadCycles.
	std::uint64_t setupCycles{};
	std::uint64_t launchCycles{};
	std::uint64_t finishCycles{};
	std::uint64_t copyCycles{};
};

// First DMA failure: controller state captured before stopping channels.
struct PicoSdDmaFailure {
	bool valid{};
	char const* stage{"none"};
	std::uint32_t manager{}, rxChannel{}, txChannel{}, events{};
	std::uint32_t managerFault{}, channelFaults{}, rxFaultType{}, txFaultType{};
	std::uint32_t rxPc{}, txPc{}, spiStatus{}, spiIrq{}, spiTxLevel{}, spiRxLevel{};
	std::uint32_t spiDmaControl{}, spiTxThreshold{}, spiRxThreshold{};
	std::uint32_t spiEnabled{}, spiSlaveEnabled{}, grfSocCon42{};
	std::uint32_t rxCodeAddress{}, txCodeAddress{};
};

PicoSdDmaFailure picoSdDmaFailure();

// Saved on the first missed IRQ *before* the GIC line is released or
// PL330's INTCLR/INTEN are changed. No MMIO is required to read this record.
struct PicoSdDmaMissedIrq {
	bool valid{};
	std::uint32_t pl330Inten{}, pl330EventStatus{}, pl330Intstatus{};
	std::uint32_t grfSocCon38{}, grfSocCon39{};
	bool gicValid{};
	bool gicEnabled{}, gicPending{}, gicActive{};
	std::uint32_t gicTarget{}, gicPriority{}, gicPmr{}, gicHppir{};

	// GICD_* bitmaps captured while PL330 INTSTATUS was still asserted.
	// Six words are used on the observed 192-line GIC. Store all 32 banks
	// to remain safe if a future GICv2 implementation exposes more lines.
	bool gicBanksValid{};

	std::uint32_t gicBankCount{};
	std::uint32_t gicEnabledBanks[32]{};
	std::uint32_t gicPendingBanks[32]{};
	std::uint32_t gicActiveBanks[32]{};
};

PicoSdDmaMissedIrq picoSdDmaMissedIrq();

// Read-only PL330 interrupt routing snapshot; safe without enabling DMA.
struct PicoSdDmaIrqRegisters {
	bool available{};
	std::uint32_t enabledEvents{};
	std::uint32_t eventStatus{}; // ES, before INTEN gating.
	std::uint32_t pendingEvents{};
	std::uint32_t managerFault{};
	std::uint32_t channelFaults{};
	std::uint32_t grfSocCon38{}; // DMAC0 MASK0/MASK1 routing (read-only).
	std::uint32_t grfSocCon39{}; // DMAC0 MASK2 routing (read-only).
};

PicoSdDmaIrqRegisters picoSdDmaIrqRegisters();

// Experimental, explicitly opted-in DMA. Default always stays PIO.
void picoSdDmaReset();
bool picoSdDmaProbe();
bool picoSdDmaEnabled();
bool picoSdDmaSetEnabled(bool enable);
// One-way handoff to a debugger: refuse while PL330 channels are busy;
// release our IRQ/event-stream ownership and disable further DMA reads.
bool picoSdDmaPrepareRamReload();
// Unsafe on a failed transfer: the destination may be partially overwritten.
// Only opt in to direct writes when the caller accepts this failure semantic.
bool picoSdDmaSetDirect(bool enabled);
bool picoSdDmaDirect();
// Experimental PL330 completion IRQs. Default is polling; IRQ wait does not
// run other cooperative tasks, it merely lets CPU0 sleep using WFE.
bool picoSdDmaSetWaitIrq(bool enabled);
bool picoSdDmaWaitIrq();
// No Group1 IRQ required; CNTKCTL event stream wakes WFE periodically.
bool picoSdDmaSetWaitEvent(bool enabled);
bool picoSdDmaWaitEvent();
bool picoSdDmaReadData(std::uint8_t* dest, std::size_t size);
PicoSpi1DmaInfo picoSdDmaInfo();
PicoSdDmaStats picoSdDmaStatistics();

} // namespace lexxos::platform::rk3506
