#pragma once

#include <array>
#include <cstddef>

#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::utility_programs::detail {

constexpr std::size_t maxUnmountTargets = 4;

struct MountTargets {
	std::array<std::array<char, lexxos::vfs::maxPathLength>, maxUnmountTargets> targets{};
	std::array<std::size_t, maxUnmountTargets> lengths{};
	std::size_t count{0};
	bool overflow{false};
};

} // namespace lexxos::apps::utility_programs::detail
