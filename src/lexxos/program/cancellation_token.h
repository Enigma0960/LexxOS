#pragma once

#include <atomic>

namespace lexxos::program {

class CancellationToken {
public:
	void request() {
		_requested.store(true, std::memory_order_release);
	}

	void reset() {
		_requested.store(false, std::memory_order_release);
	}

	[[nodiscard]] bool requested() const {
		return _requested.load(std::memory_order_acquire);
	}

private:
	std::atomic_bool _requested{false};
};

} // namespace lexxos::program
