#pragma once

#include <array>
#include <span>

#include <lexxos/hardware/products/storage_policy.h>

namespace lexxos::app::rk3506 {

// Boot-time policy snapshot and the existing media session flags. No VFS,
// scheduler, global board lookup or allocation is needed to configure it.
struct PicoSdServiceState {
	bool writeArmed{false};
	bool autoEnabled{false};
	bool autoSuppressed{false};
	bool manualReadOnly{false};
	bool writeFaulted{false};

public:
	bool configure(std::span<hardware::products::StoragePolicy const> slots);

	[[nodiscard]] std::span<hardware::products::StoragePolicy const> policy() const;
	[[nodiscard]] hardware::products::StoragePolicy const* removablePolicy() const;

	// Apply the board default at each new mount, preserving explicit RO and
	// a latched sector-write failure until removal/reboot.
	void armForMount();
	void cardRemoved();

private:
	bool _configured{false};

private:
	std::array<hardware::products::StoragePolicy, 2> _slots{};
};

} // namespace lexxos::app::rk3506
