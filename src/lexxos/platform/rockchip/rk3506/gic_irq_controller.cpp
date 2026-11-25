#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t kGicdTyper = GicIrqController::distributorBase + 0x004U;
	constexpr std::uintptr_t kGicdIsenabler0 = GicIrqController::distributorBase + 0x100U;
	constexpr std::uintptr_t kGicdIcenabler0 = GicIrqController::distributorBase + 0x180U;
	constexpr std::uint32_t kTimerBit = 1U << GicIrqController::timerSource.line;
	constexpr std::uint32_t kDmaId = GicIrqController::dmaSource.line;
	constexpr std::uint32_t kDmaBit = 1U << (kDmaId % 32U);
	constexpr std::uintptr_t kDmaEnabler = GicIrqController::distributorBase + 0x100U + (kDmaId / 32U) * 4U;
	constexpr std::uintptr_t kDmaDisabler = GicIrqController::distributorBase + 0x180U + (kDmaId / 32U) * 4U;
	constexpr std::uintptr_t kDmaClearPending = GicIrqController::distributorBase + 0x280U + (kDmaId / 32U) * 4U;
	constexpr std::uintptr_t kDmaPriority = GicIrqController::distributorBase + 0x400U + (kDmaId & ~3U);
	constexpr std::uintptr_t kDmaTarget = GicIrqController::distributorBase + 0x800U + (kDmaId & ~3U);
	constexpr std::uintptr_t kDmaTrigger = GicIrqController::distributorBase + 0xC00U + (kDmaId / 16U) * 4U;
	constexpr std::uint32_t kDmaShift = (kDmaId % 4U) * 8U;
	constexpr std::uint32_t kDmaByteMask = 0xFFU << kDmaShift;

	std::uint32_t byteAt(std::uint32_t value) noexcept {
		return (value >> kDmaShift) & 0xFFU;
	}

	std::uint32_t replaceByte(std::uint32_t word, std::uint32_t value) noexcept {
		return (word & ~kDmaByteMask) | ((value & 0xFFU) << kDmaShift);
	}

} // namespace

kernel::IrqStatus GicIrqController::domainAt(std::uint32_t index, kernel::IrqDomainInfo& output) const {
	if (index != 0U) {
		return kernel::IrqStatus::NotFound;
	}

	output = {
	    .id = domainId,
	    .name = "rk3506-gic-cpu0",
	    .vectorModel = kernel::IrqVectorModel::Fixed,
	    .runtimeSubscriptions = true,
	    .sharedSubscriptions = false,
	    .configurableTrigger = false,
	    .configurablePriority = false,
	    .configurableAffinity = false,
	};

	return kernel::IrqStatus::Ok;
}

GicIrqController::UpdateState GicIrqController::beginUpdate() noexcept {
	// CPU1/CPU2 are parked. On SMP this needs an additional remote ISR barrier.
	return _access.saveAndDisableIrq();
}

void GicIrqController::endUpdate(UpdateState state) noexcept {
	_access.restoreIrq(state);
}

kernel::IrqStatus GicIrqController::attachSource(kernel::IrqRequest const& request) {
	bool const dma = request.source == dmaSource && _allowDma;
	if (request.source != timerSource && !dma) {
		return kernel::IrqStatus::NotFound;
	}

	if (dma ? _dmaClaimed : _claimed) {
		return kernel::IrqStatus::Busy;
	}

	// The first slice inherits trigger and priority settings. Do not change
	// firmware-assigned security grouping, CPU target or the exception vector.
	if (request.trigger != kernel::IrqTrigger::Preserve || request.priority != kernel::IrqPriority::Preserve || request.cpuAffinity != kernel::anyIrqCpu) {
		return kernel::IrqStatus::Unsupported;
	}

	std::uint32_t const lines = ((_access.read(kGicdTyper) & 31U) + 1U) * 32U;
	if (lines <= request.source.line) {
		return kernel::IrqStatus::NotFound;
	}

	if (dma) {
		// NS IGROUPR is RAZ/WI on GICv2; validate ownership through the
		// readable enable bit instead. Only the explicit SD DMA owner calls us.
		if ((_access.read(kDmaEnabler) & kDmaBit) != 0U) {
			return kernel::IrqStatus::Busy;
		}

		if ((_access.read(kDmaTrigger) & (2U << ((kDmaId % 16U) * 2U))) != 0U) {
			return kernel::IrqStatus::Unsupported; // PL330 event is level-triggered.
		}

		std::uint32_t const target = byteAt(_access.read(kDmaTarget));
		if (target != 0U && target != 1U) {
			return kernel::IrqStatus::Busy; // Cannot reroute another CPU's IRQ.
		}

		_savedDmaPriority = byteAt(_access.read(kDmaPriority));
		_savedDmaTarget = target;
		_dmaFaulted = false;
		_dmaEnabled = false;
		_dmaClaimed = true;
		return kernel::IrqStatus::Ok;
	}

	// On a GICv2 with Security Extensions, IGROUPR0 is Secure-only:
	// Non-secure reads are RAZ/WI, even for a correctly configured Group1 PPI.
	// This backend claims only the OS-owned virtual timer PPI27. A pre-enabled
	// accessible line is still refused, and enabling must pass readback below.

	// A pre-enabled PPI might belong to boot firmware or an RTOS. Do not
	// silently assume ownership, mask, clear pending or rewrite its priority.
	if ((_access.read(kGicdIsenabler0) & kTimerBit) != 0U) {
		return kernel::IrqStatus::Busy;
	}

	_claimed = true;
	return kernel::IrqStatus::Ok;
}

kernel::IrqStatus GicIrqController::setLineEnabled(kernel::IrqSourceId source, bool enabled) {
	if (source == dmaSource && _allowDma) {
		if (!_dmaClaimed) {
			return kernel::IrqStatus::NotFound;
		}

		if (_dmaFaulted && enabled) {
			return kernel::IrqStatus::HardwareError;
		}

		if (_dmaEnabled == enabled) {
			return kernel::IrqStatus::Ok;
		}

		if (!enabled) {
			_access.write(kDmaDisabler, kDmaBit);
			if ((_access.read(kDmaEnabler) & kDmaBit) != 0U) {
				return kernel::IrqStatus::HardwareError;
			}

			_dmaEnabled = false;
			return kernel::IrqStatus::Ok;
		}

		// Keep other GIC targets and priorities unchanged, including every
		// other byte in the words shared with our SPI interrupt ID.
		_access.write(kDmaPriority, replaceByte(_access.read(kDmaPriority), 0xA0U));
		_access.write(kDmaTarget, replaceByte(_access.read(kDmaTarget), 1U));
		if (byteAt(_access.read(kDmaTarget)) != 1U
		    || byteAt(_access.read(kDmaPriority)) != 0xA0U) {
			_access.write(kDmaTarget, replaceByte(_access.read(kDmaTarget), _savedDmaTarget));
			_access.write(kDmaPriority, replaceByte(_access.read(kDmaPriority), _savedDmaPriority));
			return kernel::IrqStatus::HardwareError;
		}

		_access.write(kDmaClearPending, kDmaBit);
		_access.write(kDmaEnabler, kDmaBit);
		if ((_access.read(kDmaEnabler) & kDmaBit) == 0U) {
			_access.write(kDmaTarget, replaceByte(_access.read(kDmaTarget), _savedDmaTarget));
			_access.write(kDmaPriority, replaceByte(_access.read(kDmaPriority), _savedDmaPriority));
			return kernel::IrqStatus::HardwareError;
		}

		_dmaEnabled = true;
		return kernel::IrqStatus::Ok;
	}

	if (source != timerSource || !_claimed) {
		return kernel::IrqStatus::NotFound;
	}

	if (_enabled == enabled) {
		return kernel::IrqStatus::Ok;
	}

	// GIC enable registers use W1S/W1C. Touch only our PPI bit.
	_access.write(enabled ? kGicdIsenabler0 : kGicdIcenabler0, kTimerBit);

	// A Secure-only PPI is invisible to Non-secure ISENABLER reads/writes.
	// Never announce successful enable/disable if the operation was ignored.
	if (((_access.read(kGicdIsenabler0) & kTimerBit) != 0U) != enabled) {
		return kernel::IrqStatus::HardwareError;
	}

	_enabled = enabled;
	return kernel::IrqStatus::Ok;
}

kernel::IrqStatus GicIrqController::detachSource(kernel::IrqSourceId source) {
	if (source == dmaSource && _allowDma) {
		if (!_dmaClaimed) {
			return kernel::IrqStatus::NotFound;
		}

		// An in-flight callback is impossible on CPU0 inside beginUpdate.
		// Even after fault masking, verify the GIC line is genuinely off.
		if ((_access.read(kDmaEnabler) & kDmaBit) != 0U) {
			_access.write(kDmaDisabler, kDmaBit);
			if ((_access.read(kDmaEnabler) & kDmaBit) != 0U) {
				return kernel::IrqStatus::HardwareError;
			}
		}

		_access.write(kDmaTarget, replaceByte(_access.read(kDmaTarget), _savedDmaTarget));
		_access.write(kDmaPriority, replaceByte(_access.read(kDmaPriority), _savedDmaPriority));
		_dmaClaimed = false;
		_dmaEnabled = false;
		_dmaFaulted = false;

		return kernel::IrqStatus::Ok;
	}

	if (source != timerSource || !_claimed) {
		return kernel::IrqStatus::NotFound;
	}

	if (_enabled) {
		_access.write(kGicdIcenabler0, kTimerBit);
		_enabled = false;
	}

	// The caller holds IRQ masked on CPU0. No ISR can be executing there.
	_claimed = false;
	return kernel::IrqStatus::Ok;
}

std::uint32_t GicIrqController::lineCount() const noexcept {
	return ((_access.read(distributorBase + 0x004U) & 31U) + 1U) * 32U;
}

void GicIrqController::bankSnapshot(GicIrqBankSnapshot& output) const noexcept {
	output = {};
	output.lineCount = lineCount();
	output.bankCount = (output.lineCount + 31U) / 32U;
	if (output.bankCount > GicIrqBankSnapshot::maxBanks) {
		output.bankCount = GicIrqBankSnapshot::maxBanks;
	}

	for (std::uint32_t bank = 0; bank < output.bankCount; ++bank) {
		std::uintptr_t const offset = static_cast<std::uintptr_t>(bank) * 4U;
		output.enabled[bank] = _access.read(distributorBase + 0x100U + offset);
		output.pending[bank] = _access.read(distributorBase + 0x200U + offset);
		output.active[bank] = _access.read(distributorBase + 0x300U + offset);
	}
}

kernel::IrqStatus GicIrqController::securityInfo(kernel::IrqSourceId source, kernel::IrqSecurityInfo& output) const {
	if (source.domain != domainId || source.line >= lineCount()) {
		return kernel::IrqStatus::NotFound;
	}

	// CPU0 executes in Non-secure PL1 after OP-TEE hand-off. In GICv2 with
	// Security Extensions IGROUPRn is Secure-only (RAZ/WI from here). Even
	// a zero group bit is NOT evidence of Group0. Accessible enable/pending
	// bits also do not certify the security group or firmware ownership.
	output = {
	    .executionWorld = kernel::IrqExecutionWorld::NonSecure,
	    .group = kernel::IrqSecurityGroup::Unknown,
	    .readable = false,
	    .configurable = false,
	};

	return kernel::IrqStatus::Ok;
}

kernel::IrqStatus GicIrqController::snapshot(kernel::IrqSourceId source, GicIrqSnapshot& output) const {
	kernel::IrqSecurityInfo security{};
	if (securityInfo(source, security) != kernel::IrqStatus::Ok) {
		return kernel::IrqStatus::NotFound;
	}

	std::uint32_t const count = lineCount();
	if (source.line >= count) {
		return kernel::IrqStatus::NotFound;
	}

	std::uint32_t const id = source.line;
	std::uintptr_t const bank = (id / 32U) * 4U;
	std::uint32_t const bit = 1U << (id % 32U);
	std::uint32_t const shift = (id % 4U) * 8U;
	constexpr std::uintptr_t cpu = 0xFF582000U;

	output = {
	    .source = source,
	    .security = security,
	    .enabled = (_access.read(distributorBase + 0x100U + bank) & bit) != 0U,
	    .pending = (_access.read(distributorBase + 0x200U + bank) & bit) != 0U,
	    .active = (_access.read(distributorBase + 0x300U + bank) & bit) != 0U,
	    .edgeTriggered = (_access.read(distributorBase + 0xC00U + (id / 16U) * 4U) & (2U << (2U * (id % 16U)))) != 0U,
	    .targetReadable = id >= 32U,
	    .target = id >= 32U ? (_access.read(distributorBase + 0x800U + (id & ~3U)) >> shift) & 0xFFU : 0U,
	    .priority = (_access.read(distributorBase + 0x400U + (id & ~3U)) >> shift) & 0xFFU,
	    .distributorControl = _access.read(distributorBase + 0x000U),
	    .cpuControl = _access.read(cpu + 0x000U),
	    .cpuPriorityMask = _access.read(cpu + 0x004U),
	    .cpuHighestPending = _access.read(cpu + 0x018U) & 0x3FFU,
	};

	return kernel::IrqStatus::Ok;
}

void GicIrqController::maskUnacknowledgedDmaFromIsr() noexcept {
	if (!_dmaClaimed) {
		return; // Never mask a foreign line.
	}

	_access.write(kDmaDisabler, kDmaBit);
	_dmaEnabled = false;
	_dmaFaulted = true; // A fresh subscribe is required to recover.
}

} // namespace lexxos::platform::rk3506
