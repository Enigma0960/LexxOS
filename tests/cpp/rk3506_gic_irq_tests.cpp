#include <cassert>
#include <cstdint>

#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>

namespace {

using lexxos::kernel::IrqDisposition;
using lexxos::kernel::IrqDomainInfo;
using lexxos::kernel::IrqManager;
using lexxos::kernel::IrqPriority;
using lexxos::kernel::IrqRequest;
using lexxos::kernel::IrqSourceId;
using lexxos::kernel::IrqStatus;
using lexxos::kernel::IrqSubscription;
using lexxos::kernel::IrqVectorModel;
using lexxos::platform::rk3506::GicIrqController;
using lexxos::platform::rk3506::GicRegisterAccess;

constexpr auto source = GicIrqController::timerSource;
constexpr auto base = GicIrqController::distributorBase;
constexpr auto bit = 1U << source.line;

struct FakeRegisters final : GicRegisterAccess {
	std::uint32_t typer{1U};
	std::uint32_t group{bit};
	std::uint32_t enabled{};
	std::uint32_t writes{};
	std::uint32_t unexpectedWrites{};
	bool ignoreEnableWrite{};
	std::uintptr_t savedState{0x80U};
	bool inUpdate{};

	[[nodiscard]] std::uint32_t read(std::uintptr_t address) const noexcept override {
		switch (address - base) {
			case 0x004U: return typer;
			case 0x080U: return group;
			case 0x100U: return enabled;
			default    : return 0U;
		}
	}

	void write(std::uintptr_t address, std::uint32_t value) noexcept override {
		assert(inUpdate && value == bit);
		++writes;
		if (address == base + 0x100U) {
			if (!ignoreEnableWrite) {
				enabled |= value;
			}
		} else if (address == base + 0x180U) {
			enabled &= ~value;
		} else {
			++unexpectedWrites;
		}
	}

	[[nodiscard]] std::uintptr_t saveAndDisableIrq() noexcept override {
		assert(!inUpdate);
		inUpdate = true;
		return savedState;
	}

	void restoreIrq(std::uintptr_t state) noexcept override {
		assert(inUpdate && state == savedState);
		inUpdate = false;
	}
};

struct Stats {
	std::uint32_t hits{};
};

IrqDisposition handleTimer(IrqSourceId irq, void* context) noexcept {
	assert(irq == source);
	++static_cast<Stats*>(context)->hits;
	return IrqDisposition::Handled;
}

void smokeTest() {
	FakeRegisters regs{};
	GicIrqController backend{regs};
	IrqManager<2> manager{backend};
	IrqDomainInfo domain{};
	assert(manager.domainCount() == 1U);
	assert(manager.domainAt(0, domain) == IrqStatus::Ok);
	assert(domain.id == source.domain && domain.vectorModel == IrqVectorModel::Fixed);
	assert(domain.runtimeSubscriptions && !domain.configurablePriority);
	assert(manager.domainAt(1, domain) == IrqStatus::NotFound);
	Stats stats{};
	int owner{};
	IrqRequest request{.source = source, .owner = &owner, .handler = handleTimer, .userData = &stats};
	IrqSubscription handle{};
	assert(manager.subscribe(request, handle) == IrqStatus::Ok);
	assert(handle.valid() && regs.enabled == 0 && regs.writes == 0);
	assert(manager.dispatch(source) == IrqDisposition::NotHandled);
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	assert(regs.enabled == bit && regs.writes == 1);
	assert(manager.dispatch(source) == IrqDisposition::Handled && stats.hits == 1);
	assert(manager.setEnabled(handle, &owner, false) == IrqStatus::Ok);
	assert(regs.enabled == 0 && regs.writes == 2);
	assert(manager.unsubscribe(handle, &owner) == IrqStatus::Ok);
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::StaleHandle);
	assert(regs.unexpectedWrites == 0 && !regs.inUpdate);
}

void rejectsForeignSources() {
	FakeRegisters regs{};
	GicIrqController backend{regs};
	IrqManager<1> manager{backend};
	int owner{};
	Stats stats{};
	IrqRequest request{.source = source, .owner = &owner, .handler = handleTimer, .userData = &stats};
	IrqSubscription output{};
	// IGROUPR0 reads as zero in Non-secure GICv2, even for Group1 PPIs.
	regs.group = 0U;
	assert(manager.subscribe(request, output) == IrqStatus::Ok);
	assert(manager.unsubscribe(output, &owner) == IrqStatus::Ok);
	regs.group = bit;
	regs.enabled = bit;
	assert(manager.subscribe(request, output) == IrqStatus::Busy);
	regs.enabled = 0;
	request.source.line = 55U;
	assert(manager.subscribe(request, output) == IrqStatus::NotFound);
	request.source = source;
	request.priority = IrqPriority::High;
	assert(manager.subscribe(request, output) == IrqStatus::Unsupported);
	request.priority = IrqPriority::Preserve;
	assert(manager.subscribe(request, output) == IrqStatus::Ok);
	assert(manager.unsubscribe(output, &owner) == IrqStatus::Ok);
	assert(regs.writes == 0 && regs.unexpectedWrites == 0);
}

void rejectsIgnoredEnableWrite() {
	FakeRegisters regs{};
	GicIrqController backend{regs};
	IrqManager<1> manager{backend};
	int owner{};
	Stats stats{};
	IrqRequest request{.source = source, .owner = &owner, .handler = handleTimer, .userData = &stats};
	IrqSubscription handle{};
	assert(manager.subscribe(request, handle) == IrqStatus::Ok);
	regs.ignoreEnableWrite = true;
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::HardwareError);
	assert(regs.enabled == 0U);
	assert(manager.dispatch(source) == IrqDisposition::NotHandled);
	regs.ignoreEnableWrite = false;
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	assert(manager.setEnabled(handle, &owner, false) == IrqStatus::Ok);
	assert(manager.unsubscribe(handle, &owner) == IrqStatus::Ok);
}

} // namespace

int main() {
	smokeTest();
	rejectsForeignSources();
	rejectsIgnoredEnableWrite();
}
