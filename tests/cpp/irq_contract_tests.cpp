#include <cassert>
#include <cstdint>
#include <type_traits>

#include <lexxos/kernel/irq.h>

namespace {

using lexxos::kernel::IrqDisposition;
using lexxos::kernel::IrqDomainId;
using lexxos::kernel::IrqDomainInfo;
using lexxos::kernel::IrqRequest;
using lexxos::kernel::IrqService;
using lexxos::kernel::IrqSourceId;
using lexxos::kernel::IrqStatus;
using lexxos::kernel::IrqSubscription;
using lexxos::kernel::IrqVectorModel;

static_assert(std::is_abstract_v<IrqService>);
static_assert(std::is_trivially_copyable_v<IrqSourceId>);
static_assert(std::is_trivially_copyable_v<IrqSubscription>);
static_assert(!IrqSourceId{}.valid());
static_assert(IrqSourceId{IrqDomainId{1}, 0}.valid());
static_assert(IrqSourceId{IrqDomainId{1}, 0} != IrqSourceId{IrqDomainId{2}, 0});
static_assert(!IrqSubscription{}.valid());

IrqDisposition onInterrupt(IrqSourceId source, void* context) noexcept {
	if (source != IrqSourceId{IrqDomainId{1}, 27}) {
		return IrqDisposition::NotHandled;
	}
	++*static_cast<int*>(context);
	return IrqDisposition::Handled;
}

// This fake checks that a fixed vector backend can advertise subscriptions
// through a preinstalled trampoline. It is NOT a hardware IRQ implementation.
class FixedVectorBackend final : public IrqService {
public:
	[[nodiscard]] std::uint32_t domainCount() const override { return 1; }

	[[nodiscard]] IrqStatus domainAt(std::uint32_t index, IrqDomainInfo& output) const override {
		if (index != 0) {
			return IrqStatus::NotFound;
		}
		output = {.id = IrqDomainId{1}, .name = "fixed-test", .vectorModel = IrqVectorModel::Fixed, .runtimeSubscriptions = true};
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus subscribe(IrqRequest const& request, IrqSubscription& output) override {
		if (!request.source.valid() || request.owner == nullptr || request.handler == nullptr) {
			return IrqStatus::InvalidArgument;
		}

		if (request.source.domain != IrqDomainId{1}) {
			return IrqStatus::NotFound;
		}
		if (_subscription.valid()) {
			return IrqStatus::Busy;
		}

		_request = request;
		_subscription = {request.source, 1, ++_generation};
		output = _subscription;
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus setEnabled(
	    IrqSubscription const& subscription, void const* owner, bool enabled) override {
		if (!matches(subscription, owner)) {
			return IrqStatus::StaleHandle;
		}
		_enabled = enabled;
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus unsubscribe(
	    IrqSubscription const& subscription, void const* owner) override {
		if (!matches(subscription, owner)) {
			return IrqStatus::StaleHandle;
		}

		_enabled = false;
		_subscription = {};
		_request = {};
		return IrqStatus::Ok;
	}

	void trigger(IrqSourceId source) {
		if (_enabled && source == _subscription.source) {
			assert(_request.handler(source, _request.userData) == IrqDisposition::Handled);
		}
	}

private:
	[[nodiscard]] bool matches(IrqSubscription const& subscription, void const* owner) const {
		return subscription.valid() && subscription.source == _subscription.source && subscription.slot == _subscription.slot && subscription.generation == _subscription.generation && owner == _request.owner;
	}

	IrqRequest _request{};
	IrqSubscription _subscription{};
	std::uint32_t _generation{};
	bool _enabled{};
};

} // namespace

int main() {
	FixedVectorBackend controller;
	IrqDomainInfo info{};
	assert(controller.domainCount() == 1);
	assert(controller.domainAt(0, info) == IrqStatus::Ok);
	assert(info.vectorModel == IrqVectorModel::Fixed && info.runtimeSubscriptions);
	assert(controller.domainAt(1, info) == IrqStatus::NotFound);

	int owner{};
	int count{};
	IrqRequest request{
	    .source = {IrqDomainId{1}, 27},
	    .owner = &owner,
	    .handler = onInterrupt,
	    .userData = &count
    };
	IrqSubscription subscription{};
	assert(controller.subscribe(request, subscription) == IrqStatus::Ok);
	assert(subscription.valid());
	assert(controller.subscribe(request, subscription) == IrqStatus::Busy);
	controller.trigger(request.source); // Disabled until explicitly activated.
	assert(count == 0);
	assert(controller.setEnabled(subscription, &owner, true) == IrqStatus::Ok);
	controller.trigger(request.source);
	assert(count == 1);
	assert(controller.setEnabled(subscription, &count, false) == IrqStatus::StaleHandle);
	assert(controller.unsubscribe(subscription, &owner) == IrqStatus::Ok);
	controller.trigger(request.source);
	assert(count == 1);
	assert(controller.setEnabled(subscription, &owner, true) == IrqStatus::StaleHandle);

	IrqSubscription next{};
	assert(controller.subscribe(request, next) == IrqStatus::Ok);
	assert(next.generation != subscription.generation);
	assert(controller.unsubscribe(subscription, &owner) == IrqStatus::StaleHandle);
	assert(controller.unsubscribe(next, &owner) == IrqStatus::Ok);
}
