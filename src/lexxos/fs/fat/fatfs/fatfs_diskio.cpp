#include <lexxos/fs/fat/fatfs/fatfs_diskio.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <lexxos/fs/fat/fatfs/fatfs_diskio_types.h>
#include <lexxos/kernel/scheduler.h>

extern "C" {
#include <ff.h>
#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <diskio_impl.h>
#endif
}

namespace {

using lexxos::fs::fat::fatfs_backend::detail::DriveSlot;
using lexxos::fs::fat::fatfs_backend::detail::ReadAheadCache;

#if defined(LEXXOS_FATFS_DISKIO_BOUNCE_READS)
constexpr bool bounceDiskReads = LEXXOS_FATFS_DISKIO_BOUNCE_READS != 0;
#else
constexpr bool bounceDiskReads = true;
#endif
#if defined(LEXXOS_FATFS_DISKIO_YIELD_EVERY_SECTORS)
constexpr std::size_t yieldEverySectors = static_cast<std::size_t>(LEXXOS_FATFS_DISKIO_YIELD_EVERY_SECTORS);
#else
constexpr std::size_t yieldEverySectors = 16;
#endif
#if defined(LEXXOS_FAT_READ_CACHE_SECTORS)
constexpr std::size_t readAheadSectors = static_cast<std::size_t>(LEXXOS_FAT_READ_CACHE_SECTORS);
#else
constexpr std::size_t readAheadSectors = 64;
#endif
static_assert(readAheadSectors >= 1U, "FatFs read-ahead cache must keep at least one sector");
static_assert(readAheadSectors <= 64U, "FatFs read-ahead cache is intentionally capped to protect embedded RAM");

#if defined(LEXXOS_FAT_DISK_WRITE_MAX_SECTORS)
constexpr std::size_t defaultDiskWriteMaxSectors = static_cast<std::size_t>(LEXXOS_FAT_DISK_WRITE_MAX_SECTORS);
#else
constexpr std::size_t defaultDiskWriteMaxSectors = 0;
#endif
static_assert(defaultDiskWriteMaxSectors <= 64U, "FatFs disk write chunk limit is intentionally capped");

#if defined(LEXXOS_FAT_MOUNT_SLOTS)
constexpr std::size_t driveSlotCount = static_cast<std::size_t>(LEXXOS_FAT_MOUNT_SLOTS);
#else
constexpr std::size_t driveSlotCount = 2;
#endif
static_assert(driveSlotCount > 0U);

std::array<DriveSlot, driveSlotCount> g_driveSlots{};
alignas(8) ReadAheadCache<readAheadSectors> g_readAheadCache{};
std::size_t g_diskWriteMaxSectors{defaultDiskWriteMaxSectors};
lexxos::fs::fat::fatfs_backend::DiskWriteStats g_diskWriteStats{};

DSTATUS lexxosDiskInitialize(BYTE pdrv);
DSTATUS lexxosDiskStatus(BYTE pdrv);
DRESULT lexxosDiskRead(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);
DRESULT lexxosDiskWrite(BYTE pdrv, BYTE const* buff, LBA_t sector, UINT count);
DRESULT lexxosDiskIoctl(BYTE pdrv, BYTE cmd, void* buff);

#if defined(LEXXOS_PLATFORM_ESPIDF)
ff_diskio_impl_t const g_lexxosDiskIo{
    .init = lexxosDiskInitialize,
    .status = lexxosDiskStatus,
    .read = lexxosDiskRead,
    .write = lexxosDiskWrite,
    .ioctl = lexxosDiskIoctl,
};
#endif

DriveSlot* slotFor(BYTE drive) {
	if (static_cast<std::size_t>(drive) >= g_driveSlots.size()) {
		return nullptr;
	}
	return &g_driveSlots[static_cast<std::size_t>(drive)];
}

DriveSlot const* slotFor(BYTE drive, int) {
	if (static_cast<std::size_t>(drive) >= g_driveSlots.size()) {
		return nullptr;
	}
	return &g_driveSlots[static_cast<std::size_t>(drive)];
}

void invalidateReadAheadCache(BYTE drive) {
	if (g_readAheadCache.valid && g_readAheadCache.drive != drive) {
		return;
	}
	g_readAheadCache.valid = false;
	g_readAheadCache.drive = 0xFF;
	g_readAheadCache.firstSector = 0;
	g_readAheadCache.sectorCount = 0;
}

void invalidateReadAheadCacheRange(BYTE drive, std::uint64_t firstSector, std::size_t sectorCount) {
	if (!g_readAheadCache.valid || g_readAheadCache.drive != drive || sectorCount == 0) {
		return;
	}

	std::uint64_t const cacheBegin = g_readAheadCache.firstSector;
	std::uint64_t const cacheEnd = cacheBegin + static_cast<std::uint64_t>(g_readAheadCache.sectorCount);
	std::uint64_t const writeBegin = firstSector;
	std::uint64_t const writeEnd = writeBegin + static_cast<std::uint64_t>(sectorCount);
	if (writeBegin < cacheEnd && writeEnd > cacheBegin) {
		invalidateReadAheadCache(drive);
	}
}

void countWriteChunk(std::size_t chunkBlocks) {
	++g_diskWriteStats.chunks;
	g_diskWriteStats.maxChunkSectors = std::max<std::uint32_t>(
	    g_diskWriteStats.maxChunkSectors,
	    static_cast<std::uint32_t>(std::min<std::size_t>(chunkBlocks, UINT32_MAX)));
	if (chunkBlocks <= 1U) {
		++g_diskWriteStats.chunks1;
	} else if (chunkBlocks == 2U) {
		++g_diskWriteStats.chunks2;
	} else if (chunkBlocks <= 4U) {
		++g_diskWriteStats.chunks3To4;
	} else if (chunkBlocks <= 8U) {
		++g_diskWriteStats.chunks5To8;
	} else if (chunkBlocks <= 16U) {
		++g_diskWriteStats.chunks9To16;
	} else if (chunkBlocks <= 32U) {
		++g_diskWriteStats.chunks17To32;
	} else {
		++g_diskWriteStats.chunks33Plus;
	}
}

void countWriteRequest(std::size_t blockCount) {
	++g_diskWriteStats.calls;
	g_diskWriteStats.requestedSectors += static_cast<std::uint32_t>(std::min<std::size_t>(blockCount, UINT32_MAX));
	g_diskWriteStats.maxRequestSectors = std::max<std::uint32_t>(
	    g_diskWriteStats.maxRequestSectors,
	    static_cast<std::uint32_t>(std::min<std::size_t>(blockCount, UINT32_MAX)));
}

[[nodiscard]] bool readBlocksWithDiskioRecovery(
    lexxos::devices::BlockDevice& device,
    std::uint64_t firstBlock,
    void* buffer,
    std::size_t blockCount) {
	if (device.readBlocks(firstBlock, buffer, blockCount)) {
		return true;
	}

	// FatFs treats RES_ERROR as a hard disk fault.  If the underlying SD/SPI
	// transport glitched after a long stream, give the block device one chance to
	// reinitialize and retry the same request before the error reaches FatFs.
	if (!device.initialize()) {
		return false;
	}
	return device.readBlocks(firstBlock, buffer, blockCount);
}

[[nodiscard]] bool writeBlocksChunked(
    lexxos::devices::BlockDevice& device,
    std::uint64_t firstBlock,
    BYTE const* buffer,
    std::size_t blockCount) {
	std::uint64_t currentBlock = firstBlock;
	BYTE const* currentBuffer = buffer;
	std::size_t remainingBlocks = blockCount;
	while (remainingBlocks != 0U) {
		std::size_t const chunkBlocks = g_diskWriteMaxSectors == 0U
		    ? remainingBlocks
		    : std::min<std::size_t>(remainingBlocks, g_diskWriteMaxSectors);
		countWriteChunk(chunkBlocks);
		if (!device.writeBlocks(currentBlock, currentBuffer, chunkBlocks)) {
			++g_diskWriteStats.failures;
			return false;
		}
		g_diskWriteStats.writtenSectors += static_cast<std::uint32_t>(std::min<std::size_t>(chunkBlocks, UINT32_MAX));
		currentBlock += chunkBlocks;
		currentBuffer += chunkBlocks * 512U;
		remainingBlocks -= chunkBlocks;
		if (yieldEverySectors != 0U && (currentBlock - firstBlock) % yieldEverySectors == 0U) {
			lexxos::kernel::yield();
		}
	}
	return true;
}

[[nodiscard]] bool fillReadAheadCache(lexxos::devices::BlockDevice& device, BYTE drive, std::uint64_t firstSector) {
	std::uint64_t const deviceBlocks = device.blockCount();
	if (firstSector >= deviceBlocks) {
		return false;
	}

	std::uint64_t const remainingBlocks = deviceBlocks - firstSector;
	std::size_t const sectorsToRead = static_cast<std::size_t>(std::min<std::uint64_t>(
	    remainingBlocks,
	    static_cast<std::uint64_t>(readAheadSectors)));

	if (!readBlocksWithDiskioRecovery(device, firstSector, g_readAheadCache.data.data(), sectorsToRead)) {
		invalidateReadAheadCache(drive);
		return false;
	}

	g_readAheadCache.valid = true;
	g_readAheadCache.drive = drive;
	g_readAheadCache.firstSector = firstSector;
	g_readAheadCache.sectorCount = sectorsToRead;
	++g_readAheadCache.stats.fills;
	g_readAheadCache.stats.sectorsReadFromDevice += static_cast<std::uint32_t>(sectorsToRead);
	return true;
}

[[nodiscard]] bool copyFromReadAheadCache(
    BYTE drive,
    std::uint64_t sector,
    BYTE* output,
    std::size_t requestedSectors,
    std::size_t& copiedSectors) {
	copiedSectors = 0;
	if (!g_readAheadCache.valid || g_readAheadCache.drive != drive) {
		return false;
	}
	if (sector < g_readAheadCache.firstSector) {
		return false;
	}

	std::uint64_t const offset = sector - g_readAheadCache.firstSector;
	if (offset >= g_readAheadCache.sectorCount) {
		return false;
	}

	std::size_t const sectorOffset = static_cast<std::size_t>(offset);
	copiedSectors = std::min<std::size_t>(requestedSectors, g_readAheadCache.sectorCount - sectorOffset);
	std::memcpy(output, g_readAheadCache.data.data() + sectorOffset * 512U, copiedSectors * 512U);
	++g_readAheadCache.stats.hits;
	g_readAheadCache.stats.sectorsServedFromCache += static_cast<std::uint32_t>(copiedSectors);
	return true;
}

} // namespace

bool lexxos::fs::fat::fatfs_backend::acquireDrive(lexxos::devices::BlockDevice& device, BYTE& outputDrive) {
	for (std::size_t index = 0; index < g_driveSlots.size(); ++index) {
		auto& slot = g_driveSlots[index];
		if (slot.used) {
			continue;
		}
		slot.used = true;
		slot.device = &device;
		outputDrive = static_cast<BYTE>(index);
#if defined(LEXXOS_PLATFORM_ESPIDF)
		ff_diskio_register(outputDrive, &g_lexxosDiskIo);
#endif
		return true;
	}
	return false;
}

void lexxos::fs::fat::fatfs_backend::releaseDrive(BYTE drive) {
	auto* slot = slotFor(drive);
	if (slot == nullptr) {
		return;
	}
	invalidateReadAheadCache(drive);
	*slot = {};
}

lexxos::devices::BlockDevice* lexxos::fs::fat::fatfs_backend::driveDevice(BYTE drive) {
	auto* slot = slotFor(drive);
	if (slot == nullptr || !slot->used) {
		return nullptr;
	}
	return slot->device;
}

lexxos::fs::fat::fatfs_backend::DiskCacheStats lexxos::fs::fat::fatfs_backend::diskCacheStats() {
	auto result = g_readAheadCache.stats;
	result.enabled = readAheadSectors > 1U;
	result.valid = g_readAheadCache.valid;
	result.drive = g_readAheadCache.drive;
	result.firstSector = g_readAheadCache.firstSector;
	result.sectorCount = static_cast<std::uint32_t>(g_readAheadCache.sectorCount);
	result.capacitySectors = static_cast<std::uint32_t>(readAheadSectors);
	return result;
}

void lexxos::fs::fat::fatfs_backend::resetDiskCacheStats() {
	g_readAheadCache.stats = {};
}

void lexxos::fs::fat::fatfs_backend::invalidateDiskCache() {
	g_readAheadCache.valid = false;
	g_readAheadCache.drive = 0xFF;
	g_readAheadCache.firstSector = 0;
	g_readAheadCache.sectorCount = 0;
}

lexxos::fs::fat::fatfs_backend::DiskWriteStats lexxos::fs::fat::fatfs_backend::diskWriteStats() {
	auto result = g_diskWriteStats;
	result.maxSectorsPerChunk = static_cast<std::uint32_t>(std::min<std::size_t>(g_diskWriteMaxSectors, UINT32_MAX));
	return result;
}

void lexxos::fs::fat::fatfs_backend::resetDiskWriteStats() {
	g_diskWriteStats = {};
}

std::size_t lexxos::fs::fat::fatfs_backend::diskWriteMaxSectors() {
	return g_diskWriteMaxSectors;
}

bool lexxos::fs::fat::fatfs_backend::setDiskWriteMaxSectors(std::size_t sectors) {
	if (sectors > 64U) {
		return false;
	}
	g_diskWriteMaxSectors = sectors;
	return true;
}

namespace {

DSTATUS lexxosDiskInitialize(BYTE pdrv) {
	auto* device = lexxos::fs::fat::fatfs_backend::driveDevice(pdrv);
	if (device == nullptr) {
		return STA_NOINIT | STA_NODISK;
	}

	invalidateReadAheadCache(pdrv);

	if (device->isReady() || device->initialize()) {
		return device->isReadOnly() ? STA_PROTECT : 0;
	}
	return STA_NOINIT;
}

DSTATUS lexxosDiskStatus(BYTE pdrv) {
	auto* device = lexxos::fs::fat::fatfs_backend::driveDevice(pdrv);
	if (device == nullptr) {
		return STA_NOINIT | STA_NODISK;
	}
	return device->isReady() ? (device->isReadOnly() ? STA_PROTECT : 0) : STA_NOINIT;
}

DRESULT lexxosDiskRead(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
	auto* device = lexxos::fs::fat::fatfs_backend::driveDevice(pdrv);
	if (device == nullptr || buff == nullptr || count == 0) {
		return RES_PARERR;
	}
	if (!device->isReady()) {
		return RES_NOTRDY;
	}

	if constexpr (!bounceDiskReads) {
		if (readBlocksWithDiskioRecovery(*device, static_cast<std::uint64_t>(sector), buff, static_cast<std::size_t>(count))) {
			return RES_OK;
		}
		return RES_ERROR;
	}

	std::uint64_t currentSector = static_cast<std::uint64_t>(sector);
	std::size_t remainingSectors = static_cast<std::size_t>(count);
	BYTE* output = buff;
	while (remainingSectors != 0) {
		std::size_t copiedSectors = 0;
		if (!copyFromReadAheadCache(pdrv, currentSector, output, remainingSectors, copiedSectors)) {
			++g_readAheadCache.stats.misses;
			if (!fillReadAheadCache(*device, pdrv, currentSector)) {
				return RES_ERROR;
			}
			if (!copyFromReadAheadCache(pdrv, currentSector, output, remainingSectors, copiedSectors)) {
				return RES_ERROR;
			}
		}

		currentSector += copiedSectors;
		remainingSectors -= copiedSectors;
		output += copiedSectors * 512U;
		if (yieldEverySectors != 0 && (static_cast<std::size_t>(count) - remainingSectors) % yieldEverySectors == 0U) {
			lexxos::kernel::yield();
		}
	}
	return RES_OK;
}

DRESULT lexxosDiskWrite(BYTE pdrv, BYTE const* buff, LBA_t sector, UINT count) {
	auto* device = lexxos::fs::fat::fatfs_backend::driveDevice(pdrv);
	if (device == nullptr || buff == nullptr || count == 0) {
		return RES_PARERR;
	}

	if (!device->isReady()) {
		return RES_NOTRDY;
	}

	if (device->isReadOnly()) {
		return RES_WRPRT;
	}

	invalidateReadAheadCacheRange(pdrv, static_cast<std::uint64_t>(sector), static_cast<std::size_t>(count));
	countWriteRequest(static_cast<std::size_t>(count));
	return writeBlocksChunked(*device, static_cast<std::uint64_t>(sector), buff, static_cast<std::size_t>(count)) ? RES_OK : RES_ERROR;
}

DRESULT lexxosDiskIoctl(BYTE pdrv, BYTE cmd, void* buff) {
	auto* device = lexxos::fs::fat::fatfs_backend::driveDevice(pdrv);
	if (device == nullptr) {
		return RES_PARERR;
	}
	if (!device->isReady()) {
		return RES_NOTRDY;
	}

	switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			if (buff == nullptr) {
				return RES_PARERR;
			}
			*static_cast<LBA_t*>(buff) = static_cast<LBA_t>(device->blockCount());
			return RES_OK;
		case GET_SECTOR_SIZE:
			if (buff == nullptr) {
				return RES_PARERR;
			}
			*static_cast<WORD*>(buff) = static_cast<WORD>(device->blockSizeBytes());
			return RES_OK;
		case GET_BLOCK_SIZE:
			if (buff == nullptr) {
				return RES_PARERR;
			}
			*static_cast<DWORD*>(buff) = 1;
			return RES_OK;
		default:
			return RES_PARERR;
	}
}

} // namespace

#if !defined(LEXXOS_PLATFORM_ESPIDF)
extern "C" DSTATUS disk_initialize(BYTE pdrv) {
	return lexxosDiskInitialize(pdrv);
}

extern "C" DSTATUS disk_status(BYTE pdrv) {
	return lexxosDiskStatus(pdrv);
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
	return lexxosDiskRead(pdrv, buff, sector, count);
}

extern "C" DRESULT disk_write(BYTE pdrv, BYTE const* buff, LBA_t sector, UINT count) {
	return lexxosDiskWrite(pdrv, buff, sector, count);
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
	return lexxosDiskIoctl(pdrv, cmd, buff);
}

extern "C" DWORD get_fattime(void) {
	constexpr DWORD year = 2026 - 1980;
	constexpr DWORD month = 1;
	constexpr DWORD day = 1;
	constexpr DWORD hour = 0;
	constexpr DWORD minute = 0;
	constexpr DWORD second = 0;
	return (year << 25U) | (month << 21U) | (day << 16U) | (hour << 11U) | (minute << 5U) | (second / 2U);
}
#endif
