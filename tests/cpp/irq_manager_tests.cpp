#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

#include <lexxos/kernel/irq_manager.h>

namespace {

using lexxos::kernel::IrqController;
using lexxos::kernel::IrqDisposition;
using lexxos::kernel::IrqDomainInfo;
using lexxos::kernel::IrqManager;
using lexxos::kernel::IrqPriority;
using lexxos::kernel::IrqRequest;
using lexxos::kernel::IrqService;
using lexxos::kernel::IrqSharing;
using lexxos::kernel::IrqSourceId;
using lexxos::kernel::IrqStatus;
using lexxos::kernel::IrqSubscription;
using lexxos::kernel::IrqTrigger;
using lexxos::kernel::IrqVectorModel;

constexpr IrqSourceId timer{{1}, 27};
constexpr IrqSourceId dma{{1}, 55};
constexpr IrqSourceId gpio{{2}, 3};
constexpr IrqSourceId staticOnly{{3}, 1};
constexpr IrqSourceId sdk{{4}, 5};

struct FakeController final : IrqController {
	struct Line {
		IrqSourceId source{};
		bool claimed{};
		bool enabled{};
	};

	std::array<Line, 8> lines{};
	bool updating{};
	bool inIsr{};
	bool failAttach{};
	bool failEnable{};
	bool failDetach{};
	unsigned attachCount{};
	unsigned detachCount{};
	unsigned enableCount{};
	unsigned disableCount{};
	unsigned updates{};

	[[nodiscard]] std::uint32_t domainCount() const override { return 4; }

	[[nodiscard]] IrqStatus domainAt(std::uint32_t index, IrqDomainInfo& output) const override {
		if (index >= domainCount()) {
			return IrqStatus::NotFound;
		}

		switch (index) {
			case 0:
				output = {.id = {1}, .name = "gic", .vectorModel = IrqVectorModel::Relocatable, .runtimeSubscriptions = true, .sharedSubscriptions = true, .configurableTrigger = true, .configurablePriority = true};
				return IrqStatus::Ok;
			case 1:
				output = {
				    .id = {2},
				    .name = "gpio-bank",
				    .parent = {{1}, 11},
				    .vectorModel = IrqVectorModel::Fixed,
				    .runtimeSubscriptions = true
                };
				return IrqStatus::Ok;
			case 2:
				output = {.id = {3}, .name = "rom-vectors", .vectorModel = IrqVectorModel::Fixed};
				return IrqStatus::Ok;
			case 3:
				output = {.id = {4}, .name = "espidf", .vectorModel = IrqVectorModel::SdkManaged, .runtimeSubscriptions = true, .sharedSubscriptions = true};
				return IrqStatus::Ok;
		}

		return IrqStatus::NotFound;
	}

	[[nodiscard]] UpdateState beginUpdate() noexcept override {
		assert(!updating && !inIsr);
		updating = true;
		++updates;
		return 0xABCDEFU;
	}

	void endUpdate(UpdateState state) noexcept override {
		assert(updating && state == 0xABCDEFU);
		updating = false;
	}

	[[nodiscard]] Line* lineFor(IrqSourceId source) {
		for (auto& line : lines) {
			if (line.claimed && line.source == source) {
				return &line;
			}
		}

		return nullptr;
	}

	[[nodiscard]] IrqStatus attachSource(IrqRequest const& request) override {
		assert(updating && !inIsr);
		if (request.source == IrqSourceId{{1}, 42}) {
			return IrqStatus::AccessDenied; // Secure firmware-owned line.
		}
		if (failAttach) {
			return IrqStatus::HardwareError;
		}

		if (request.source.domain.value == 1 && request.source.line > 100) {
			return IrqStatus::NotFound;
		}

		if (request.source.domain.value == 2 && request.source.line > 31) {
			return IrqStatus::NotFound;
		}

		assert(lineFor(request.source) == nullptr);
		for (auto& line : lines) {
			if (!line.claimed) {
				line = {.source = request.source, .claimed = true, .enabled = false};
				++attachCount;
				return IrqStatus::Ok;
			}
		}

		return IrqStatus::NoCapacity;
	}

	[[nodiscard]] IrqStatus setLineEnabled(IrqSourceId source, bool enabled) override {
		assert(updating && !inIsr);
		if (failEnable) {
			return IrqStatus::HardwareError;
		}

		Line* line = lineFor(source);
		if (line == nullptr) {
			return IrqStatus::NotFound;
		}

		assert(line->enabled != enabled);
		line->enabled = enabled;
		++(enabled ? enableCount : disableCount);
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus detachSource(IrqSourceId source) override {
		assert(updating && !inIsr);
		if (failDetach) {
			return IrqStatus::HardwareError;
		}

		Line* line = lineFor(source);
		if (line == nullptr) {
			return IrqStatus::NotFound;
		}

		*line = {};
		++detachCount;
		return IrqStatus::Ok;
	}

	template <std::size_t Count>
	[[nodiscard]] IrqDisposition fire(IrqManager<Count>& manager, IrqSourceId source) {
		assert(!updating && !inIsr);
		inIsr = true;
		Line* const line = lineFor(source);
		IrqDisposition result = IrqDisposition::NotHandled;
		if (line != nullptr && line->enabled) {
			result = manager.dispatch(source);
		}

		inIsr = false;
		return result;
	}
};

struct CallbackState {
	unsigned calls{};
	IrqSourceId lastSource{};
	IrqDisposition disposition{IrqDisposition::Handled};
};

IrqDisposition callback(IrqSourceId source, void* context) noexcept {
	auto& state = *static_cast<CallbackState*>(context);
	++state.calls;
	state.lastSource = source;
	return state.disposition;
}

IrqRequest request(IrqSourceId source, void const* owner, CallbackState& state,
    IrqSharing sharing = IrqSharing::Exclusive) {
	return {.source = source, .owner = owner, .handler = callback, .userData = &state, .sharing = sharing};
}

void testBasicLifecycle() {
	FakeController controller;
	IrqManager<2> manager{controller};
	IrqService& service = manager;
	int owner{};
	int stranger{};
	CallbackState state{};
	IrqSubscription handle{};
	IrqRequest irq = request(timer, &owner, state);

	assert(service.domainCount() == 4);
	IrqDomainInfo info{};
	assert(service.domainAt(0, info) == IrqStatus::Ok && info.id == timer.domain);
	assert(service.domainAt(1, info) == IrqStatus::Ok && info.parent == IrqSourceId({{1}, 11}));
	assert(service.domainAt(4, info) == IrqStatus::NotFound);
	assert(service.subscribe(irq, handle) == IrqStatus::Ok);
	assert(handle.valid() && handle.slot == 1 && handle.generation == 1);
	assert(controller.attachCount == 1 && !controller.lineFor(timer)->enabled);
	assert(controller.fire(manager, timer) == IrqDisposition::NotHandled && state.calls == 0);
	assert(service.setEnabled(handle, &stranger, true) == IrqStatus::AccessDenied);
	assert(service.unsubscribe(handle, &stranger) == IrqStatus::AccessDenied);
	assert(service.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	assert(service.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	assert(controller.enableCount == 1);
	assert(controller.fire(manager, timer) == IrqDisposition::Handled && state.calls == 1);
	assert(state.lastSource == timer);
	assert(controller.fire(manager, dma) == IrqDisposition::NotHandled);
	assert(service.setEnabled(handle, &owner, false) == IrqStatus::Ok);
	assert(controller.disableCount == 1);
	assert(controller.fire(manager, timer) == IrqDisposition::NotHandled && state.calls == 1);
	assert(service.unsubscribe(handle, &owner) == IrqStatus::Ok);
	assert(controller.detachCount == 1 && controller.lineFor(timer) == nullptr);
	assert(service.unsubscribe(handle, &owner) == IrqStatus::StaleHandle);
	assert(service.setEnabled(handle, &owner, true) == IrqStatus::StaleHandle);

	IrqSubscription replacement{};
	assert(service.subscribe(irq, replacement) == IrqStatus::Ok);
	assert(replacement.slot == handle.slot && replacement.generation != handle.generation);
	assert(service.unsubscribe(handle, &owner) == IrqStatus::StaleHandle);
	assert(service.unsubscribe(replacement, &owner) == IrqStatus::Ok);
	assert(!controller.updating);
}

void testSharedAndExclusive() {
	FakeController controller;
	IrqManager<3> manager{controller};
	int system{};
	int guest{};
	CallbackState first{}, second{.disposition = IrqDisposition::Reschedule}, third{};
	IrqRequest firstRequest = request(dma, &system, first, IrqSharing::Shared);
	IrqRequest secondRequest = request(dma, &guest, second, IrqSharing::Shared);
	IrqSubscription a{}, b{}, c{};
	assert(manager.subscribe(firstRequest, a) == IrqStatus::Ok);
	assert(manager.subscribe(secondRequest, b) == IrqStatus::Ok);
	assert(controller.attachCount == 1);
	assert(manager.subscribe(request(dma, &guest, third), c) == IrqStatus::Busy);
	assert(manager.setEnabled(a, &system, true) == IrqStatus::Ok);
	assert(manager.setEnabled(b, &guest, true) == IrqStatus::Ok);
	assert(controller.enableCount == 1);
	assert(controller.fire(manager, dma) == IrqDisposition::Reschedule);
	assert(first.calls == 1 && second.calls == 1);
	assert(manager.setEnabled(a, &system, false) == IrqStatus::Ok);
	assert(controller.disableCount == 0);
	assert(controller.fire(manager, dma) == IrqDisposition::Reschedule && first.calls == 1 && second.calls == 2);
	assert(manager.unsubscribe(b, &guest) == IrqStatus::Ok);
	assert(controller.disableCount == 1 && controller.detachCount == 0);
	assert(manager.unsubscribe(a, &system) == IrqStatus::Ok);
	assert(controller.detachCount == 1);

	assert(manager.subscribe(request(timer, &system, first), a) == IrqStatus::Ok);
	assert(manager.subscribe(request(timer, &guest, second, IrqSharing::Shared), b) == IrqStatus::Busy);
	assert(manager.unsubscribe(a, &system) == IrqStatus::Ok);

	// A single IRQ has a single hardware configuration even when shared.
	firstRequest.priority = IrqPriority::High;
	secondRequest.priority = IrqPriority::Low;
	assert(manager.subscribe(firstRequest, a) == IrqStatus::Ok);
	assert(manager.subscribe(secondRequest, b) == IrqStatus::Busy);
	secondRequest.priority = IrqPriority::High;
	assert(manager.subscribe(secondRequest, b) == IrqStatus::Ok);
	assert(manager.unsubscribe(a, &system) == IrqStatus::Ok);
	assert(manager.unsubscribe(b, &guest) == IrqStatus::Ok);
}

void testValidationAndRollback() {
	FakeController controller;
	IrqManager<1> manager{controller};
	int owner{};
	CallbackState state{};
	IrqSubscription output{
	    {{9}, 91},
	    8, 19
    };
	IrqSubscription const untouched = output;
	IrqRequest irq = request(timer, &owner, state);

	IrqRequest invalid = irq;
	invalid.owner = nullptr;
	assert(manager.subscribe(invalid, output) == IrqStatus::InvalidArgument);
	invalid = irq;
	invalid.handler = nullptr;
	assert(manager.subscribe(invalid, output) == IrqStatus::InvalidArgument);
	invalid = irq;
	invalid.source = {{0}, 27};
	assert(manager.subscribe(invalid, output) == IrqStatus::InvalidArgument);
	invalid.source = {{99}, 27};
	assert(manager.subscribe(invalid, output) == IrqStatus::NotFound);
	invalid.source = staticOnly;
	assert(manager.subscribe(invalid, output) == IrqStatus::Unsupported);
	invalid.source = gpio;
	invalid.sharing = IrqSharing::Shared;
	assert(manager.subscribe(invalid, output) == IrqStatus::Unsupported);
	invalid.sharing = IrqSharing::Exclusive;
	invalid.priority = IrqPriority::High;
	assert(manager.subscribe(invalid, output) == IrqStatus::Unsupported);
	invalid = request(sdk, &owner, state);
	invalid.trigger = IrqTrigger::RisingEdge;
	assert(manager.subscribe(invalid, output) == IrqStatus::Unsupported);
	invalid = irq;
	invalid.cpuAffinity = 1;
	assert(manager.subscribe(invalid, output) == IrqStatus::Unsupported);
	invalid = irq;
	invalid.source = {{1}, 42};
	assert(manager.subscribe(invalid, output) == IrqStatus::AccessDenied);
	invalid.source = {{1}, 105};
	assert(manager.subscribe(invalid, output) == IrqStatus::NotFound);
	assert(output.slot == untouched.slot && output.generation == untouched.generation);

	controller.failAttach = true;
	assert(manager.subscribe(irq, output) == IrqStatus::HardwareError);
	assert(controller.attachCount == 0 && output.source == untouched.source && output.slot == untouched.slot && output.generation == untouched.generation);
	controller.failAttach = false;
	assert(manager.subscribe(irq, output) == IrqStatus::Ok);
	IrqSubscription noCapacityOutput{};
	assert(manager.subscribe(request(dma, &owner, state), noCapacityOutput) == IrqStatus::NoCapacity);
	assert(!noCapacityOutput.valid());

	controller.failEnable = true;
	assert(manager.setEnabled(output, &owner, true) == IrqStatus::HardwareError);
	assert(!controller.lineFor(timer)->enabled);
	controller.failEnable = false;
	assert(manager.setEnabled(output, &owner, true) == IrqStatus::Ok);
	controller.failEnable = true;
	assert(manager.setEnabled(output, &owner, false) == IrqStatus::HardwareError);
	assert(controller.lineFor(timer)->enabled);
	controller.failEnable = false;
	controller.failDetach = true;
	assert(manager.unsubscribe(output, &owner) == IrqStatus::HardwareError);
	assert(controller.lineFor(timer)->enabled);
	assert(controller.fire(manager, timer) == IrqDisposition::Handled && state.calls == 1);
	controller.failDetach = false;
	assert(manager.unsubscribe(output, &owner) == IrqStatus::Ok);
	assert(!controller.updating);
}

void testCascadeAndUnclaimed() {
	FakeController controller;
	IrqManager<2> manager{controller};
	int owner{};
	CallbackState state{.disposition = IrqDisposition::NotHandled};
	IrqSubscription handle{};
	assert(manager.subscribe(request(gpio, &owner, state), handle) == IrqStatus::Ok);
	assert(manager.setEnabled(handle, &owner, true) == IrqStatus::Ok);
	assert(controller.fire(manager, gpio) == IrqDisposition::NotHandled && state.calls == 1);
	assert(manager.dispatch(timer) == IrqDisposition::NotHandled);
	assert(manager.dispatch({}) == IrqDisposition::NotHandled);
	assert(manager.unsubscribe(handle, &owner) == IrqStatus::Ok);
}

} // namespace

int main() {
	testBasicLifecycle();
	testSharedAndExclusive();
	testValidationAndRollback();
	testCascadeAndUnclaimed();
}
