#pragma once

#include <atomic>
#include <cstdint>

namespace lexxos::kernel::detail {

struct Counter {
	std::atomic<std::uint32_t> value;
	std::uint32_t maximum;
};

bool acquire(void* state);
bool release(void* state);

} // namespace lexxos::kernel::detail
