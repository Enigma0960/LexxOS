/* RK3506 SDMMC stage 8d: explicit read-only FAT32 mount from GPT partition 3.
 * There are NO SDMMC write commands here. The partition BlockDevice rejects
 * writes and the existing FatFs diskio layer returns RES_WRPRT.
 * No automatic mount, no formatting, no GPT modification.
 */
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/fs/fat/fat_volume.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform::rk3506 {

bool inspectSdmmcPartitions(lexxos::os::AppContext& context);

namespace {

	constexpr std::uint32_t kSectorSize = 512;
	constexpr std::string_view kBlockName{"sd0p3"};
	constexpr std::string_view kMountPath{"/mnt/luckfox"};

	std::uint16_t le16(std::uint8_t const* bytes) {
		return std::uint16_t(bytes[0]) | (std::uint16_t(bytes[1]) << 8U);
	}

	std::uint32_t le32(std::uint8_t const* bytes) {
		return std::uint32_t(le16(bytes)) | (std::uint32_t(le16(bytes + 2)) << 16U);
	}

	std::uint64_t le64(std::uint8_t const* bytes) {
		return std::uint64_t(le32(bytes)) | (std::uint64_t(le32(bytes + 4)) << 32U);
	}

	void printDecimal(lexxos::os::AppContext& context, std::uint64_t value) {
		char digits[21]{};
		std::size_t position = sizeof(digits) - 1U;
		do {
			digits[--position] = static_cast<char>('0' + value % 10U);
			value /= 10U;
		} while (value != 0);

		context.write(digits + position);
	}

	// Validate this really is a FAT32 *volume boot sector*, not just a GPT entry.
	// In particular, reject FAT12/FAT16, ext4, blank rootfs and fake "FAT32" labels.
	bool validFat32BootSector(std::uint8_t const* sector, std::uint64_t partitionSectors) {
		if (sector[510] != 0x55U || sector[511] != 0xAAU || (sector[0] != 0xEBU && sector[0] != 0xE9U) || le16(sector + 11) != kSectorSize) {
			return false;
		}

		auto const sectorsPerCluster = sector[13];
		auto const reserved = le16(sector + 14);
		auto const fatCount = sector[16];
		auto const fatSectors = le32(sector + 36);
		auto const totalSectors = le32(sector + 32);
		if (sectorsPerCluster == 0 || sectorsPerCluster > 128U || (sectorsPerCluster & (sectorsPerCluster - 1U)) != 0 || reserved == 0 || (fatCount != 1 && fatCount != 2) || le16(sector + 17) != 0 || le16(sector + 22) != 0 || le16(sector + 19) != 0 || fatSectors == 0 || totalSectors == 0 || totalSectors > partitionSectors || le32(sector + 44) < 2U) {
			return false;
		}

		std::uint64_t const metadataSectors =
		    std::uint64_t(reserved) + std::uint64_t(fatCount) * fatSectors;
		if (metadataSectors >= totalSectors) {
			return false;
		}

		std::uint64_t const clusters =
		    (std::uint64_t(totalSectors) - metadataSectors) / sectorsPerCluster;
		return clusters >= 65525U && clusters < 0x0FFFFFF5U;
	}

	class ReadOnlyPartition final : public lexxos::devices::BlockDevice {
	public:
		bool bind(lexxos::devices::BlockDevice& device, std::uint64_t start,
		    std::uint64_t blocks) {
			if (!device.isReady() || !device.isReadOnly() || blocks == 0 || start >= device.blockCount() || blocks > device.blockCount() - start) {
				return false;
			}

			_device = &device;
			_start = start;
			_blocks = blocks;
			return true;
		}

		bool initialize() override { return isReady(); }

		bool isReady() const override {
			return _device != nullptr && _device->isReady() && _device->isReadOnly();
		}

		bool isReadOnly() const override { return true; }

		std::uint32_t blockSizeBytes() const override { return kSectorSize; }

		std::uint64_t blockCount() const override { return isReady() ? _blocks : 0; }

		bool readBlocks(std::uint64_t first, void* destination, std::size_t count) override {
			if (!isReady() || destination == nullptr || count == 0 || first >= _blocks || count > _blocks - first) {
				return false;
			}
			return _device->readBlocks(_start + first, destination, count);
		}

		bool writeBlocks(std::uint64_t, void const*, std::size_t) override {
			return false;
		}

	private:
		lexxos::devices::BlockDevice* _device{};
		std::uint64_t _start{};
		std::uint64_t _blocks{};
	};

	ReadOnlyPartition g_partition{};

	bool fail(lexxos::os::AppContext& context, char const* why) {
		context.write("sdmount: ");
		context.writeLine(why);
		return false;
	}

	bool mountReadOnly(lexxos::os::AppContext& context) {
		if (lexxos::fs::fat::isMounted(kMountPath)) {
			context.writeLine("sdmount: /mnt/luckfox already mounted read-only");
			return true;
		}

		auto* disk = context.vfs().blockDevice("sd0");
		if (disk == nullptr || !disk->isReady() || !disk->isReadOnly() || disk->blockSizeBytes() != kSectorSize) {
			return fail(context, "run 'sdmmc probe' and 'sdmmc scan' first");
		}

		alignas(4) std::uint8_t header[kSectorSize]{};
		alignas(4) std::uint8_t entrySector[kSectorSize]{};
		if (!disk->readBlocks(1, header, 1)) {
			return fail(context, "cannot read verified GPT header");
		}

		constexpr char signature[] = "EFI PART";
		for (std::size_t i = 0; i < 8; ++i) {
			if (header[i] != static_cast<std::uint8_t>(signature[i])) {
				return fail(context, "GPT signature changed since scan");
			}
		}

		auto const entryCount = le32(header + 80);
		auto const entrySize = le32(header + 84);
		auto const entryArrayStart = le64(header + 72);
		if (entryCount < 3U || entrySize != 128U || entryArrayStart < 2U || entryArrayStart >= disk->blockCount()) {
			return fail(context, "GPT has no supported partition 3");
		}

		// GPT is already CRC-validated by sdmmc scan; re-read the partition entry
		// and check its bounds so the offset device cannot leave the parent disk.
		constexpr std::uint32_t entryIndex = 2U;
		auto const entryByteOffset = std::uint64_t(entryIndex) * entrySize;
		auto const entryLba = entryArrayStart + entryByteOffset / kSectorSize;
		auto const entryOffset = static_cast<std::size_t>(entryByteOffset % kSectorSize);
		if (entryLba >= disk->blockCount() || entryOffset > kSectorSize - 128U || !disk->readBlocks(entryLba, entrySector, 1)) {
			return fail(context, "cannot read GPT partition 3 entry");
		}

		auto const* entry = entrySector + entryOffset;
		bool empty = true;
		for (std::size_t i = 0; i < 16; ++i) {
			empty &= (entry[i] == 0);
		}

		auto const start = le64(entry + 32);
		auto const last = le64(entry + 40);
		if (empty || start > last || start >= disk->blockCount() || last >= disk->blockCount()) {
			return fail(context, "GPT partition 3 is absent or out of bounds");
		}

		std::uint64_t const blocks = last - start + 1U;
		alignas(4) std::uint8_t boot[kSectorSize]{};
		if (!disk->readBlocks(start, boot, 1)) {
			return fail(context, "cannot read partition 3 boot sector");
		}

		if (!validFat32BootSector(boot, blocks)) {
			return fail(context,
			    "partition 3 is not FAT32 (may be ext4); nothing mounted or written");
		}

		// The VFS/DeviceManager keep pointers to this static object. Never rebind
		// an existing mounted partition, even if a new scan changes GPT metadata.
		if (!g_partition.bind(*disk, start, blocks)) {
			return fail(context, "cannot bind read-only partition device");
		}

		if (context.devices().blockDevice(kBlockName) == nullptr) {
			if (!context.devices().registerBlockDevice(kBlockName,
			        "RK3506 GPT partition 3 (read-only FAT32)", g_partition)) {
				return fail(context, "DeviceManager has no available block-device slot");
			}
		}

		if (!context.vfs().registerBlockDevice(kBlockName, &g_partition)) {
			return fail(context, "VFS has no available block-device slot");
		}

		auto const mountResult = lexxos::fs::fat::mount(context.vfs(), kBlockName, kMountPath);
		if (mountResult != lexxos::fs::fat::FatMountStatus::Ok && mountResult != lexxos::fs::fat::FatMountStatus::AlreadyMounted) {
			context.write("sdmount: FatFs mount failed: ");
			context.writeLine(lexxos::fs::fat::toString(mountResult));
			return false;
		}

		context.write("sdmount: /dev/sd0p3 => /mnt/luckfox FAT32 READ ONLY; start LBA=");
		printDecimal(context, start);
		context.write(" blocks=");
		printDecimal(context, blocks);
		context.writeLine("");
		return true;
	}

	int run(lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& inv,
	    lexxos::program::ParsedArguments const&) {
		auto const action = inv.argc >= 2 ? std::string_view(inv.argv[1]) : "mount";

		if (action == "mount") {
			return mountReadOnly(context) ? 0 : 1;
		}

		if (action == "inspect") {
			return inspectSdmmcPartitions(context) ? 0 : 1;
		}

		if (action == "status") {
			context.write("sdmount: /mnt/luckfox ");
			context.writeLine(lexxos::fs::fat::isMounted(kMountPath) ? "mounted ro" : "not mounted");
			return 0;
		}

		if (action == "unmount") {
			if (!lexxos::fs::fat::isMounted(kMountPath)) {
				context.writeLine("sdmount: /mnt/luckfox is not mounted");
				return 0;
			}

			auto const result = lexxos::fs::fat::unmount(context.vfs(), kMountPath);
			context.write("sdmount: unmount ");
			context.writeLine(lexxos::vfs::toString(result));
			return result == lexxos::vfs::UnmountStatus::Ok ? 0 : 1;
		}

		context.writeLine("usage: sdmount [mount | inspect | status | unmount]");
		return 1;
	}

} // namespace

bool sdmmcFatMounted() {
	return lexxos::fs::fat::isMounted(kMountPath);
}

void registerSdmmcFatProgram(lexxos::program::ProgramManager& programs) {
	programs.registerProgram({
	    .name = "sdmount",
	    .help = "mount RK3506 GPT partition 3 as read-only FAT32",
	    .description = "Explicit read-only FAT32 mount; no block writes or auto mount",
	    .usage = "sdmount [mount | inspect | status | unmount]",
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 16384,
	                .priority = 1,
	                },
	    .allowExtraArguments = true,
	    .enabled = true,
	});
}

} // namespace lexxos::platform::rk3506
