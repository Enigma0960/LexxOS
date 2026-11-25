#pragma once

#include <atomic>
#include <cstdint>

// Shared between the foreground focus router and the guest VM. No display
// pointers escape to applications. A running guest may release/reacquire mode.
namespace lexxos::app::guest_graphics_mode {

inline std::atomic<std::uint32_t> ownerProcessId{0};

inline bool acquire(std::uint32_t processId) {
	if (processId == 0) {
		return false;
	}

	std::uint32_t expected = 0;
	return ownerProcessId.compare_exchange_strong(expected, processId,
	           std::memory_order_acq_rel)
	    || expected == processId;
}

inline void release(std::uint32_t processId) {
	std::uint32_t expected = processId;
	(void) ownerProcessId.compare_exchange_strong(expected, 0,
	    std::memory_order_acq_rel);
}

inline bool owns(std::uint32_t processId) {
	return processId != 0 && ownerProcessId.load(std::memory_order_acquire) == processId;
}

} // namespace lexxos::app::guest_graphics_mode
