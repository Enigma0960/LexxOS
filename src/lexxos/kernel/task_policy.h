#pragma once

#include <cstdint>
#include <limits>

#include <lexxos/kernel/scheduler.h>

namespace lexxos::kernel {

// These checks express LexxOS task semantics, independent of FreeRTOS,
// ESP-IDF, the host simulator and any particular CPU family.
[[nodiscard]] constexpr bool validTaskCoreAffinity(int affinity, int coreCount) noexcept {
	return coreCount > 0
	    && (affinity == anyTaskCore || (affinity >= 0 && affinity < coreCount));
}

[[nodiscard]] constexpr bool validTaskPriority(std::uint32_t priority, std::uint32_t priorityLevels) noexcept {
	return priorityLevels != 0U && priority < priorityLevels;
}

// LexxOS expresses stack sizes in bytes; vanilla FreeRTOS expects words.
// Do not silently truncate or wrap the requested size.
[[nodiscard]] constexpr bool taskStackDepthFromBytes(
    std::uint32_t requestedBytes,
    std::uint32_t wordBytes,
    std::uint64_t minimumWords,
    std::uint64_t maximumWords,
    std::uint32_t& depth) noexcept {
	if (requestedBytes == 0U || wordBytes == 0U || maximumWords == 0U || minimumWords > maximumWords) {
		return false;
	}

	auto words = (static_cast<std::uint64_t>(requestedBytes) + wordBytes - 1U) / wordBytes;
	if (words < minimumWords) {
		words = minimumWords;
	}

	if (words > maximumWords || words > std::numeric_limits<std::uint32_t>::max() || words * wordBytes > std::numeric_limits<std::uint32_t>::max()) {
		return false;
	}

	depth = static_cast<std::uint32_t>(words);
	return true;
}

} // namespace lexxos::kernel
