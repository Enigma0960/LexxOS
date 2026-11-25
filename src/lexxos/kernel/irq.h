#pragma once

#include <cstdint>

namespace lexxos::kernel {

// 0 is reserved: domains are assigned by product/SoC composition, not by drivers.
struct IrqDomainId {
	std::uint32_t value{};

	[[nodiscard]] constexpr bool valid() const { return value != 0U; }

	[[nodiscard]] constexpr bool operator==(IrqDomainId const&) const = default;
};

// A local line in a domain. A GPIO bank can be a child domain of a GIC/NVIC line.
// Line 0 is valid; a source is invalid only when its domain is invalid.
struct IrqSourceId {
	IrqDomainId domain{};
	std::uint32_t line{};

	[[nodiscard]] constexpr bool valid() const { return domain.valid(); }

	[[nodiscard]] constexpr bool operator==(IrqSourceId const&) const = default;
};

// World/group are independent. Unknown does not mean Group0 or inaccessible.
// The backend must never infer security ownership from a RAZ/WI group register.
enum class IrqExecutionWorld : std::uint8_t {
	Unknown,
	NonSecure,
	Secure,
	NotApplicable,
};

enum class IrqSecurityGroup : std::uint8_t {
	Unknown,
	Group0,
	Group1,
};

struct IrqSecurityInfo {
	IrqExecutionWorld executionWorld{IrqExecutionWorld::Unknown};
	IrqSecurityGroup group{IrqSecurityGroup::Unknown};
	bool readable{}; // May group ownership be established by reading its register?
	bool configurable{}; // Does the caller have authority to change grouping?
};

enum class IrqVectorModel : std::uint8_t {
	Fixed, // Linked/ROM table, optionally with a preinstalled trampoline.
	Relocatable, // Vector entries may be changed by the platform backend.
	SdkManaged, // Vendor SDK/RTOS owns the vector and interrupt allocator.
};

struct IrqDomainInfo {
	IrqDomainId id{};
	char const* name{nullptr}; // Stable for the lifetime of the service.
	IrqSourceId parent{}; // Invalid for a root domain; valid for a cascaded domain.
	IrqVectorModel vectorModel{IrqVectorModel::Fixed};
	bool runtimeSubscriptions{false}; // True for Fixed + registered trampoline too.
	bool sharedSubscriptions{false};
	bool configurableTrigger{false};
	bool configurablePriority{false};
	bool configurableAffinity{false};
};

enum class IrqTrigger : std::uint8_t {
	Preserve, // Do not change the setting inherited from firmware/another owner.
	RisingEdge,
	FallingEdge,
	BothEdges,
	LevelHigh,
	LevelLow,
};

enum class IrqPriority : std::uint8_t {
	Preserve, // Do not program GIC/NVIC/SDK priority registers.
	Low,
	Normal,
	High,
	Critical,
};

enum class IrqSharing : std::uint8_t {
	Exclusive,
	Shared,
};

// The handler is called in interrupt context, with no allocation or blocking.
// For shared lines every subscriber must inspect/acknowledge its own device.
// Reschedule implies Handled; the handler first performs an ISR-safe task wake.
enum class IrqDisposition : std::uint8_t {
	NotHandled,
	Handled,
	Reschedule,
};

using IrqHandler = IrqDisposition (*)(IrqSourceId source, void* userData) noexcept;

inline constexpr std::int32_t anyIrqCpu = -1;

struct IrqRequest {
	IrqSourceId source{};
	void const* owner{nullptr}; // Stable privileged driver identity, not a user token.
	IrqHandler handler{nullptr};
	void* userData{nullptr};
	IrqSharing sharing{IrqSharing::Exclusive};
	IrqTrigger trigger{IrqTrigger::Preserve};
	IrqPriority priority{IrqPriority::Preserve};
	std::int32_t cpuAffinity{anyIrqCpu};
};

// Opaque, generation-checked subscription. slot=0 and generation=0 are invalid.
// Caller must not construct or modify handles received from the IRQ service.
struct IrqSubscription {
	IrqSourceId source{};
	std::uint32_t slot{};
	std::uint32_t generation{};

	[[nodiscard]] constexpr bool valid() const {
		return source.valid() && slot != 0U && generation != 0U;
	}
};

enum class IrqStatus : std::uint8_t {
	Ok,
	InvalidArgument,
	Unsupported,
	NotReady,
	Busy,
	AccessDenied,
	NoCapacity,
	NotFound,
	StaleHandle,
	HardwareError,
};

// This is an injected *contract*, not a global ISR or a concrete dispatcher.
// All methods below are task/initialization-context operations; never call them
// from an ISR. Platform backends implement registration and vector trampolines.
class IrqService {
public:
	virtual ~IrqService() = default;

	[[nodiscard]] virtual std::uint32_t domainCount() const = 0;
	[[nodiscard]] virtual IrqStatus domainAt(std::uint32_t index, IrqDomainInfo& output) const = 0;

	// Read-only capability query; does not claim the interrupt or read IAR.
	// Default keeps existing platform services source-compatible.
	[[nodiscard]] virtual IrqStatus securityInfo(IrqSourceId, IrqSecurityInfo&) const {
		return IrqStatus::Unsupported;
	}

	// On success output is valid and initially DISABLED. No unowned GIC/NVIC/SDK
	// source is reconfigured; failure must leave the hardware and output unchanged.
	[[nodiscard]] virtual IrqStatus subscribe(IrqRequest const& request, IrqSubscription& output) = 0;

	// Enables/disables this subscription, not another subscriber on a shared line.
	// The backend owns the transition of the underlying hardware line when the
	// first subscriber becomes enabled or the last subscriber is disabled.
	[[nodiscard]] virtual IrqStatus setEnabled(
	    IrqSubscription const& subscription, void const* owner, bool enabled) = 0;

	// Must stop future calls and quiesce any in-flight callback before returning;
	// then the owner may destroy userData. Must not affect other subscribers.
	[[nodiscard]] virtual IrqStatus unsubscribe(
	    IrqSubscription const& subscription, void const* owner) = 0;
};

} // namespace lexxos::kernel
