#pragma once

#include <cstdint>
#include <limits>

namespace lexxos::kernel {

inline constexpr std::uint32_t waitForever = std::numeric_limits<std::uint32_t>::max();

} // namespace lexxos::kernel
