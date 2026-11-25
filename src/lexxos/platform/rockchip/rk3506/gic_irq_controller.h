#pragma once

#include <cstdint>

#include <lexxos/kernel/irq_manager.h>
#include <lexxos/platform/rockchip/rk3506/gic_register_access.h>

namespace lexxos::platform::rk3506 {

// MMIO/CPU critical-section primitives are injected so this controller can
// be tested without an ARM toolchain or a live GIC. Only the native adapter
// may access hardware; it must preserve the caller's CPSR.I state.

// Read-only, register-derived state; no IAR read, ACK or GIC mutation.
// Values can be RAZ for secure lines: zero does not certify the physical state.
struct GicIrqSnapshot {
	kernel::IrqSourceId source{};
	kernel::IrqSecurityInfo security{};
	bool enabled{};
	bool pending{};
	bool active{};
	bool edgeTriggered{};
	bool targetReadable{}; // SGI/PPI targets are banked, unlike SPIs.
	std::uint32_t target{};
	std::uint32_t priority{};
	std::uint32_t distributorControl{};
	std::uint32_t cpuControl{};
	std::uint32_t cpuPriorityMask{};
	std::uint32_t cpuHighestPending{}; // HPPIR, read only: NOT IAR/ACK.
};

// 32 banks cover every architectural GICv2 INTID 0..1019; on RK3506 the
// GICD_TYPER currently reports 192 INTIDs (6 banks). Read-only and CPU0-local.
struct GicIrqBankSnapshot {
	static constexpr std::uint32_t maxBanks = 32U;
	std::uint32_t lineCount{};
	std::uint32_t bankCount{};
	std::uint32_t enabled[maxBanks]{};
	std::uint32_t pending[maxBanks]{};
	std::uint32_t active[maxBanks]{};
};

// CPU0-only GICv2 adapter. Only PPI27 and optionally DMAC0 IRQ148 are claimable.
// The DMAC0 second IRQ149 and other lines remain READ-ONLY diagnostics.
// RK3506 DTB: dma-controller@ff000000 interrupts <0 116 4>, <0 117 4>.
// GIC SPI identifiers in the DTB require +32 to obtain absolute INTIDs.
class GicIrqController final : public kernel::IrqController {
public:
	static constexpr kernel::IrqDomainId domainId{1U};
	static constexpr kernel::IrqSourceId timerSource{domainId, 27U};
	static constexpr kernel::IrqSourceId dmaSource{domainId, 148U};
	static constexpr kernel::IrqSourceId dmaAbortSource{domainId, 149U}; // Diagnostic only; second DMAC0 output.
	static constexpr std::uintptr_t distributorBase = 0xFF581000U;

	explicit GicIrqController(GicRegisterAccess& access, bool allowDma = false) noexcept
	    : _access(access)
	    , _allowDma(allowDma) {}

	[[nodiscard]] std::uint32_t domainCount() const override { return 1U; }

	[[nodiscard]] std::uint32_t lineCount() const noexcept;
	void bankSnapshot(GicIrqBankSnapshot& output) const noexcept;

	[[nodiscard]] kernel::IrqStatus domainAt(std::uint32_t index, kernel::IrqDomainInfo& output) const override;
	[[nodiscard]] kernel::IrqStatus securityInfo(kernel::IrqSourceId source, kernel::IrqSecurityInfo& output) const override;
	[[nodiscard]] kernel::IrqStatus snapshot(kernel::IrqSourceId source, GicIrqSnapshot& output) const;
	[[nodiscard]] UpdateState beginUpdate() noexcept override;
	void endUpdate(UpdateState state) noexcept override;
	[[nodiscard]] kernel::IrqStatus attachSource(kernel::IrqRequest const& request) override;
	[[nodiscard]] kernel::IrqStatus setLineEnabled(kernel::IrqSourceId source, bool enabled) override;
	[[nodiscard]] kernel::IrqStatus detachSource(kernel::IrqSourceId source) override;

	// Called ONLY after DMA148 dispatch reported NotHandled and before GICC EOI.
	// Do not invoke IrqService methods from an ISR. A faulted line cannot be
	// re-enabled until its owning subscription has been detached and recreated.
	void maskUnacknowledgedDmaFromIsr() noexcept;

	[[nodiscard]] bool dmaFaulted() const noexcept { return _dmaFaulted; }

	[[nodiscard]] bool dmaClaimed() const noexcept { return _dmaClaimed; }

private:
	GicRegisterAccess& _access;
	bool _allowDma{};
	bool _claimed{};
	bool _enabled{};
	bool volatile _dmaClaimed{}; // Updated with IRQ masked on CPU0.
	bool volatile _dmaEnabled{}; // CPU0 ISR can fault-mask asynchronously.
	bool volatile _dmaFaulted{};
	std::uint32_t _savedDmaPriority{};
	std::uint32_t _savedDmaTarget{};
};

} // namespace lexxos::platform::rk3506

// Implemented by the native RK3506 IRQ runtime when managed timer is on.
extern "C" bool rk3506_gic_irq_snapshot(std::uint32_t id, lexxos::platform::rk3506::GicIrqSnapshot* output);
extern "C" bool rk3506_gic_irq_bank_snapshot(lexxos::platform::rk3506::GicIrqBankSnapshot* output);
extern "C" std::uint32_t rk3506_gic_irq_delivery_count(std::uint32_t id);
