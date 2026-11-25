#include <cassert>
#include <cstdint>

#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>

namespace {

using lexxos::kernel::IrqDisposition;
using lexxos::kernel::IrqExecutionWorld;
using lexxos::kernel::IrqManager;
using lexxos::kernel::IrqRequest;
using lexxos::kernel::IrqSecurityGroup;
using lexxos::kernel::IrqSecurityInfo;
using lexxos::kernel::IrqSourceId;
using lexxos::kernel::IrqStatus;
using lexxos::kernel::IrqSubscription;
using lexxos::platform::rk3506::GicIrqController;
using lexxos::platform::rk3506::GicRegisterAccess;
constexpr auto timer = GicIrqController::timerSource;
constexpr auto dma = GicIrqController::dmaSource;
static_assert(dma.line == 148U && GicIrqController::dmaAbortSource.line == 149U);
static_assert(dma.line / 32U == 4U && dma.line % 32U == 20U);
constexpr std::uintptr_t base = GicIrqController::distributorBase;
constexpr std::uint32_t bit = 1U << (dma.line % 32U);
constexpr std::uint32_t shift = (dma.line % 4U) * 8U;

struct Fake final : GicRegisterAccess {
	std::uint32_t enable0{};
	std::uint32_t enable4{};
	std::uint32_t foreignEnable1{0x0000F00FU}; // Must remain unchanged.
	std::uint32_t target{0x00332200U};
	std::uint32_t priority{0x12345678U};
	std::uint32_t trigger{};
	std::uint32_t writes{};
	std::uint32_t badWrites{};
	bool ignoreEnable{};
	bool updating{};

	[[nodiscard]] std::uint32_t read(std::uintptr_t a) const noexcept override {
		switch (a - base) {
			case 0x004U: return 5U;
			case 0x080U: return 0U; // IGROUPR RAZ in Non-secure GICv2.
			case 0x100U: return enable0;
			case 0x104U: return foreignEnable1;
			case 0x110U: return enable4; // GICD_ISENABLER4, INTID 148.
			case 0x494U: return priority; // GICD_IPRIORITYR37.
			case 0x894U: return target; // GICD_ITARGETSR37.
			case 0xC24U: return trigger; // GICD_ICFGR9.
			default    : return 0;
		}
	}

	void write(std::uintptr_t a, std::uint32_t value) noexcept override {
		assert(updating || (a == base + 0x190U && value == bit)); // IRQ fault masking bypasses task critical section.
		++writes;
		switch (a - base) {
			case 0x100U: enable0 |= value; break;
			case 0x180U: enable0 &= ~value; break;
			case 0x110U:
				if (!ignoreEnable) {
					enable4 |= value;
				}

				break;
			case 0x190U: enable4 &= ~value; break;
			case 0x290U: assert(value == bit); break;
			case 0x494U: priority = value; break;
			case 0x894U: target = value; break;
			default    : ++badWrites; break;
		}
	}

	[[nodiscard]] std::uintptr_t saveAndDisableIrq() noexcept override {
		assert(!updating);
		updating = true;
		return 0x80U;
	}

	void restoreIrq(std::uintptr_t s) noexcept override {
		assert(updating && s == 0x80U);
		updating = false;
	}
};

struct Hits {
	unsigned count{};
};

IrqDisposition onDma(IrqSourceId id, void* p) noexcept {
	assert(id == dma);
	++static_cast<Hits*>(p)->count;
	return IrqDisposition::Handled;
}

IrqDisposition onTimer(IrqSourceId id, void* p) noexcept {
	assert(id == timer);
	++static_cast<Hits*>(p)->count;
	return IrqDisposition::Handled;
}

IrqRequest request(Hits& hits, int& owner) {
	return {.source = dma, .owner = &owner, .handler = onDma, .userData = &hits};
}

void lifecycle() {
	Fake hw;
	GicIrqController controller(hw, true);
	IrqManager<2> manager(controller);
	Hits hits;
	int owner{};
	IrqSubscription dmaHandle{}, timerHandle{};
	Hits ticks;
	int timerOwner{};
	IrqRequest timerRequest{.source = timer, .owner = &timerOwner, .handler = onTimer, .userData = &ticks};
	assert(manager.subscribe(timerRequest, timerHandle) == IrqStatus::Ok);
	assert(manager.setEnabled(timerHandle, &timerOwner, true) == IrqStatus::Ok);
	assert(hw.enable0 == (1U << timer.line));
	assert(manager.subscribe(request(hits, owner), dmaHandle) == IrqStatus::Ok);
	assert(hw.writes == 1U); // The timer alone was enabled.
	assert(manager.setEnabled(dmaHandle, &owner, true) == IrqStatus::Ok);
	assert(hw.enable4 == bit);
	assert((hw.target >> shift & 0xFFU) == 1U);
	assert((hw.priority >> shift & 0xFFU) == 0xA0U);
	assert(manager.dispatch(dma) == IrqDisposition::Handled && hits.count == 1U);
	assert(manager.setEnabled(dmaHandle, &owner, false) == IrqStatus::Ok);
	assert(hw.enable4 == 0U);
	assert(manager.unsubscribe(dmaHandle, &owner) == IrqStatus::Ok);
	assert(hw.enable0 == (1U << timer.line) && hw.foreignEnable1 == 0x0000F00FU); // DMA never touches PPI27 or bank1.
	assert(manager.dispatch(timer) == IrqDisposition::Handled && ticks.count == 1U);
	assert(manager.unsubscribe(timerHandle, &timerOwner) == IrqStatus::Ok);
	assert(hw.enable0 == 0U);
	assert(hw.target == 0x00332200U && hw.priority == 0x12345678U);
	assert(hw.badWrites == 0U && !hw.updating);
}

void safety() {
	Fake hw;
	GicIrqController controller(hw, true);
	IrqManager<2> manager(controller);
	Hits hits;
	int owner{}, foreign{};
	IrqSubscription handle{};
	auto req = request(hits, owner);
	hw.enable4 = bit;
	assert(manager.subscribe(req, handle) == IrqStatus::Busy);
	hw.enable4 = 0U;
	hw.trigger = 2U << ((dma.line % 16U) * 2U);
	assert(manager.subscribe(req, handle) == IrqStatus::Unsupported);
	hw.trigger = 0U;
	hw.target = (hw.target & ~(0xFFU << shift)) | (2U << shift);
	assert(manager.subscribe(req, handle) == IrqStatus::Busy);
	hw.target &= ~(0xFFU << shift);
	assert(manager.subscribe(req, handle) == IrqStatus::Ok);
	assert(manager.unsubscribe(handle, &foreign) == IrqStatus::AccessDenied);
	hw.ignoreEnable = true;
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::HardwareError);
	assert(hw.enable4 == 0U && hw.target == 0x00332200U);
	hw.ignoreEnable = false;
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	controller.maskUnacknowledgedDmaFromIsr();
	assert(controller.dmaFaulted() && hw.enable4 == 0U);
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::Ok); // Manager sees existing subscription as enabled.
	assert(manager.setEnabled(handle, &owner, false) == IrqStatus::Ok);
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::HardwareError);
	assert(manager.unsubscribe(handle, &owner) == IrqStatus::Ok);
	assert(!controller.dmaFaulted());
	IrqSubscription newer{};
	assert(manager.subscribe(req, newer) == IrqStatus::Ok);
	assert(newer.generation != handle.generation);
	assert(manager.unsubscribe(newer, &owner) == IrqStatus::Ok);
}

void visibilityAndAbortDiagnostics() {
	Fake hw;
	GicIrqController controller(hw, true);
	IrqManager<2> manager(controller);
	IrqSecurityInfo security{};
	assert(manager.securityInfo(dma, security) == IrqStatus::Ok);
	assert(security.executionWorld == IrqExecutionWorld::NonSecure);
	assert(security.group == IrqSecurityGroup::Unknown);
	assert(!security.readable && !security.configurable);
	assert(manager.securityInfo(GicIrqController::dmaAbortSource, security) == IrqStatus::Ok);
	lexxos::platform::rk3506::GicIrqSnapshot snap{};
	assert(controller.snapshot(GicIrqController::dmaAbortSource, snap) == IrqStatus::Ok);
	assert(snap.source == GicIrqController::dmaAbortSource && !snap.enabled);
	assert(controller.snapshot({GicIrqController::domainId, 57U}, snap) == IrqStatus::Ok);
	assert(controller.snapshot({GicIrqController::domainId, 191U}, snap) == IrqStatus::Ok);
	assert(controller.snapshot({GicIrqController::domainId, 192U}, snap) == IrqStatus::NotFound);
	assert(controller.lineCount() == 192U);
	lexxos::platform::rk3506::GicIrqBankSnapshot banks{};
	controller.bankSnapshot(banks);
	assert(banks.lineCount == 192U && banks.bankCount == 6U);
	assert(banks.enabled[0] == 0U && banks.pending[5] == 0U);
	Hits hits;
	int owner{};
	IrqRequest req{.source = GicIrqController::dmaAbortSource, .owner = &owner, .handler = onDma, .userData = &hits};
	IrqSubscription subscription{};
	assert(manager.subscribe(req, subscription) == IrqStatus::NotFound);
	assert(!subscription.valid() && hw.writes == 0U);
}

void optIn() {
	Fake hw;
	GicIrqController controller(hw); // Original timer-only backend.
	IrqManager<2> manager(controller);
	Hits hits;
	int owner{};
	IrqSubscription handle{};
	assert(manager.subscribe(request(hits, owner), handle) == IrqStatus::NotFound);
	assert(!handle.valid() && hw.writes == 0U);
	assert(manager.dispatch(timer) == IrqDisposition::NotHandled);
}

} // namespace

int main() {
	lifecycle();
	safety();

	visibilityAndAbortDiagnostics();

	optIn();
}
