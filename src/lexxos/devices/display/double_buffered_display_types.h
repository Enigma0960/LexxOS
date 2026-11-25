#pragma once

#include <cstdint>

namespace lexxos::devices {

struct DoubleBufferedDisplayTiming {
	std::uint64_t copyTicks{};
	std::uint64_t drawTicks{};
	std::uint64_t queueTicks{};
	std::uint64_t presentTicks{};
	std::uint32_t copiedFrames{};
	std::uint64_t copiedBytes{};
	std::uint32_t fullSyncs{};
	std::uint32_t partialSyncs{};
	std::uint32_t skippedSyncs{};
	std::uint32_t preparedFrames{};
	std::uint32_t presentedFrames{};
	std::uint64_t presentedRegions{};
	std::uint64_t presentedPixels{};
};

struct DisplayFrameSynchronization {
	void* context{};
	std::uintptr_t (*enter)(void*){};
	void (*leave)(void*, std::uintptr_t){};
};

} // namespace lexxos::devices
