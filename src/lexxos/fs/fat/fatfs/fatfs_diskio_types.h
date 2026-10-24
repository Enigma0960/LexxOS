#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/fs/fat/fatfs/fatfs_diskio.h>

namespace lexxos::fs::fat::fatfs_backend::detail {

struct DriveSlot {
	bool used{false};
	lexxos::devices::BlockDevice* device{nullptr};
};

template <std::size_t ReadAheadSectors>
struct ReadAheadCache {
	static_assert(ReadAheadSectors >= 1U);

	bool valid{false};
	BYTE drive{0xFF};
	std::uint64_t firstSector{0};
	std::size_t sectorCount{0};
	std::array<std::uint8_t, ReadAheadSectors * 512U> data{};
	DiskCacheStats stats{};
};

} // namespace lexxos::fs::fat::fatfs_backend::detail
