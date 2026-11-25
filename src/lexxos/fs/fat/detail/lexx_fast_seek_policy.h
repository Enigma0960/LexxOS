#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::fs::fat::detail {

// One 8-KiB CLMT, shared by the one pinned .lexx reader on this volume.
inline constexpr std::size_t lexxClusterMapWords = 2048U;
inline constexpr std::uint64_t lexxFastSeekMinBytes = 4U * 1024U * 1024U;

[[nodiscard]] constexpr bool isLexxPackagePath(std::string_view path) {
	if (path.size() <= 5U || path[path.size() - 5U] != '.') {
		return false;
	}

	for (std::size_t i = 0; i < 4U; ++i) {
		char c = path[path.size() - 4U + i];
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}

		if (c != "lexx"[i]) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] constexpr bool useLexxClusterMap(std::uint64_t bytes) {
	return bytes >= lexxFastSeekMinBytes;
}

} // namespace lexxos::fs::fat::detail
