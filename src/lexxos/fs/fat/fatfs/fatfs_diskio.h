#pragma once

#include <cstdint>

#include <lexxos/devices/storage/block_device.h>

extern "C" {
#include <ff.h>

#include <diskio.h>
}

namespace lexxos::fs::fat::fatfs_backend {

struct DiskWriteStats {
	std::uint32_t maxSectorsPerChunk{0};
	std::uint32_t calls{0};
	std::uint32_t chunks{0};
	std::uint32_t failures{0};
	std::uint32_t requestedSectors{0};
	std::uint32_t writtenSectors{0};
	std::uint32_t maxRequestSectors{0};
	std::uint32_t maxChunkSectors{0};
	std::uint32_t chunks1{0};
	std::uint32_t chunks2{0};
	std::uint32_t chunks3To4{0};
	std::uint32_t chunks5To8{0};
	std::uint32_t chunks9To16{0};
	std::uint32_t chunks17To32{0};
	std::uint32_t chunks33Plus{0};
};

struct DiskCacheStats {
	bool enabled{false};
	bool valid{false};
	std::uint8_t drive{0xFF};
	std::uint64_t firstSector{0};
	std::uint32_t sectorCount{0};
	std::uint32_t capacitySectors{0};
	std::uint32_t fills{0};
	std::uint32_t hits{0};
	std::uint32_t misses{0};
	std::uint32_t sectorsReadFromDevice{0};
	std::uint32_t sectorsServedFromCache{0};
};

[[nodiscard]] bool acquireDrive(lexxos::devices::BlockDevice& device, BYTE& outputDrive);
void releaseDrive(BYTE drive);
[[nodiscard]] lexxos::devices::BlockDevice* driveDevice(BYTE drive);
[[nodiscard]] DiskCacheStats diskCacheStats();
void resetDiskCacheStats();
void invalidateDiskCache();

[[nodiscard]] DiskWriteStats diskWriteStats();
void resetDiskWriteStats();
[[nodiscard]] std::size_t diskWriteMaxSectors();
[[nodiscard]] bool setDiskWriteMaxSectors(std::size_t sectors);

} // namespace lexxos::fs::fat::fatfs_backend
