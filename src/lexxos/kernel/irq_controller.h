#pragma once

#include <cstdint>

#include <lexxos/kernel/irq.h>

namespace lexxos::kernel {

class IrqController {
public:
	using UpdateState = std::uintptr_t;

	virtual ~IrqController() = default;

	[[nodiscard]] virtual std::uint32_t domainCount() const = 0;
	[[nodiscard]] virtual IrqStatus domainAt(std::uint32_t index, IrqDomainInfo& output) const = 0;

	[[nodiscard]] virtual IrqStatus securityInfo(IrqSourceId, IrqSecurityInfo&) const {
		return IrqStatus::Unsupported;
	}

	// Only task/init context may enter an update. Prevent IRQ dispatch to this
	// manager while registrations or enable bits are being changed. Before
	// returning, also wait for any in-flight callback on another CPU; an SMP
	// backend must provide that guarantee or must not expose this service.
	// Nested updates are not performed by IrqManager. Save/restore IRQ state,
	// never unconditionally enable interrupts in endUpdate().
	[[nodiscard]] virtual UpdateState beginUpdate() noexcept = 0;
	virtual void endUpdate(UpdateState state) noexcept = 0;

	// Called once for the first subscriber on a source. Must validate source,
	// secure/firmware ownership and request configuration. On success the
	// source is owned but DISABLED; on failure there are no side effects.
	[[nodiscard]] virtual IrqStatus attachSource(IrqRequest const& request) = 0;

	// Called only on first enable / last disable, not once per subscriber.
	// Must preserve state on error and never touch a different IRQ source.
	[[nodiscard]] virtual IrqStatus setLineEnabled(IrqSourceId source, bool enabled) = 0;

	// Called once for the last subscriber. Atomically disable, quiesce and
	// release ONLY this source. On failure ownership/state must be unchanged.
	// A platform which cannot guarantee this must fail closed rather than
	// return Ok with a callback still in flight.
	[[nodiscard]] virtual IrqStatus detachSource(IrqSourceId source) = 0;
};

} // namespace lexxos::kernel
