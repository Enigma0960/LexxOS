/* RK3506 stage 8c: validate primary GPT and expose the existing CMD17 PIO
 * reader through LexxOS BlockDevice. READ ONLY: there are NO write commands,
 * no mount and no automatic probing. A user must explicitly run sdmmc scan.
 * Initial support deliberately limits cards to 4 GiB-ish (<= 0x800000 LBAs).
 */
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform::rk3506 {

bool sdmmcReady();
bool sdmmcReadSector(std::uint32_t lba, std::uint8_t* dest);

namespace {

	constexpr std::uint32_t kSectorBytes = 512U;
	constexpr std::uint32_t kMaximumLba = 0x007FFFFFU; // Stage 8 readSector cap.
	constexpr std::uint32_t kMaximumEntryBytes = 64U * 1024U;
	constexpr std::uint32_t kMaxDisplayedPartitions = 16U;

	std::uint64_t g_sectorCount{};
	bool g_gptValidated{};
	bool g_attached{};

	std::uint32_t le32(std::uint8_t const* p) {
		return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8U) | (std::uint32_t(p[2]) << 16U) | (std::uint32_t(p[3]) << 24U);
	}

	std::uint64_t le64(std::uint8_t const* p) {
		return std::uint64_t(le32(p)) | (std::uint64_t(le32(p + 4)) << 32U);
	}

	std::uint32_t crc32Continue(std::uint32_t crc, std::uint8_t const* bytes, std::size_t size) {
		for (std::size_t i = 0; i < size; ++i) {
			crc ^= bytes[i];
			for (unsigned bit = 0; bit < 8; ++bit) {
				crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
			}
		}

		return crc;
	}

	void decimal(lexxos::os::AppContext& context, std::uint64_t value) {
		char buf[21]{};
		std::size_t pos = sizeof(buf) - 1U;
		do {
			buf[--pos] = char('0' + value % 10U);
			value /= 10U;
		} while (value != 0);

		context.write(buf + pos);
	}

	void hex32(lexxos::os::AppContext& context, std::uint32_t value) {
		constexpr char digits[] = "0123456789abcdef";
		char buf[] = "0x00000000";
		for (unsigned i = 0; i < 8; ++i) {
			buf[2 + i] = digits[(value >> (28U - 4U * i)) & 15U];
		}

		context.write(buf);
	}

	bool error(lexxos::os::AppContext& context, char const* why) {
		context.write("SDMMC scan: ");
		context.writeLine(why);
		return false;
	}

	// Storage references a static object: lifetime extends past the scan command.
	class Rk3506ReadOnlyBlockDevice final : public lexxos::devices::BlockDevice {
	public:
		bool initialize() override { return isReady(); } // Never reinitialize live SDMMC.

		bool isReady() const override { return sdmmcReady() && g_gptValidated; }

		bool isReadOnly() const override { return true; }

		std::uint32_t blockSizeBytes() const override { return kSectorBytes; }

		std::uint64_t blockCount() const override { return isReady() ? g_sectorCount : 0; }

		bool readBlocks(std::uint64_t first, void* destination, std::size_t count) override {
			if (!isReady() || !destination || count == 0 || first >= g_sectorCount || count > g_sectorCount - first) {
				return false;
			}

			auto* target = static_cast<std::uint8_t*>(destination);
			for (std::size_t i = 0; i < count; ++i) {
				if (!sdmmcReadSector(static_cast<std::uint32_t>(first + i), target + i * kSectorBytes)) {
					g_gptValidated = false; // Fail closed after an I/O error.
					return false;
				}
			}

			return true;
		}

		bool writeBlocks(std::uint64_t, void const*, std::size_t) override {
			return false; // Even a FAT or VFS bug cannot write via this adapter.
		}
	};

	Rk3506ReadOnlyBlockDevice g_blockDevice;

	bool allZero(std::uint8_t const* bytes, std::size_t count) {
		for (std::size_t i = 0; i < count; ++i) {
			if (bytes[i] != 0) {
				return false;
			}
		}

		return true;
	}

	// Primary GPT only. Validate CRC32 of both the header and partition-entry
	// array before making the block device visible to other LexxOS components.
	bool validateGpt(lexxos::os::AppContext& context) {
		alignas(4) std::uint8_t sector[kSectorBytes]{};
		if (!sdmmcReadSector(0, sector)) {
			return error(context, "cannot read protective MBR");
		}

		if (sector[510] != 0x55 || sector[511] != 0xAA) {
			return error(context, "invalid protective MBR signature");
		}

		bool protective = false;
		for (unsigned i = 0; i < 4; ++i) {
			if (sector[446U + i * 16U + 4U] == 0xEEU) {
				protective = true;
			}
		}

		if (!protective) {
			return error(context, "no protective GPT partition (0xee)");
		}

		if (!sdmmcReadSector(1, sector)) {
			return error(context, "cannot read GPT header at LBA 1");
		}

		constexpr std::uint8_t signature[] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
		for (unsigned i = 0; i < sizeof(signature); ++i) {
			if (sector[i] != signature[i]) {
				return error(context, "GPT signature missing");
			}
		}

		auto const revision = le32(sector + 8);
		auto const headerSize = le32(sector + 12);
		auto const expectedHeaderCrc = le32(sector + 16);
		auto const currentLba = le64(sector + 24);
		auto const alternateLba = le64(sector + 32);
		auto const firstUsable = le64(sector + 40);
		auto const lastUsable = le64(sector + 48);
		auto const entriesStart = le64(sector + 72);
		auto const entryCount = le32(sector + 80);
		auto const entrySize = le32(sector + 84);
		auto const expectedArrayCrc = le32(sector + 88);

		if (revision != 0x00010000U || headerSize < 92U || headerSize > kSectorBytes || currentLba != 1U || alternateLba < 35U || alternateLba > kMaximumLba || firstUsable < 34U || firstUsable > lastUsable || lastUsable >= alternateLba || entriesStart < 2U || entriesStart >= firstUsable || entryCount == 0U || entrySize != 128U || std::uint64_t(entryCount) * entrySize > kMaximumEntryBytes) {
			return error(context, "unsupported or invalid GPT header fields");
		}

		// The CRC field is treated as zero while computing the header checksum.
		std::uint32_t crc = 0xFFFFFFFFU;
		crc = crc32Continue(crc, sector, 16U);
		constexpr std::uint8_t zeros[4]{};
		crc = crc32Continue(crc, zeros, 4U);
		crc = crc32Continue(crc, sector + 20U, headerSize - 20U);
		if ((crc ^ 0xFFFFFFFFU) != expectedHeaderCrc) {
			return error(context, "GPT header CRC32 mismatch");
		}

		std::uint64_t const tableBytes = std::uint64_t(entryCount) * entrySize;
		std::uint64_t const tableSectors = (tableBytes + kSectorBytes - 1U) / kSectorBytes;
		if (entriesStart + tableSectors > firstUsable || entriesStart + tableSectors > alternateLba) {
			return error(context, "GPT partition array bounds invalid");
		}

		crc = 0xFFFFFFFFU;
		std::uint32_t foundPartitions = 0;
		for (std::uint64_t sectorIndex = 0; sectorIndex < tableSectors; ++sectorIndex) {
			if (!sdmmcReadSector(static_cast<std::uint32_t>(entriesStart + sectorIndex), sector)) {
				return error(context, "GPT partition array read failed");
			}

			std::uint64_t const remaining = tableBytes - sectorIndex * kSectorBytes;
			auto const validBytes = static_cast<std::size_t>(remaining < kSectorBytes ? remaining : kSectorBytes);
			crc = crc32Continue(crc, sector, validBytes);
			// A GPT entry is exactly 128 bytes in this deliberately restricted reader.
			for (std::size_t pos = 0; pos + 128U <= validBytes; pos += 128U) {
				auto const* entry = sector + pos;
				if (allZero(entry, 16U)) {
					continue;
				}

				auto const first = le64(entry + 32U);
				auto const last = le64(entry + 40U);
				if (first < firstUsable || last > lastUsable || first > last) {
					return error(context, "GPT entry LBA outside usable disk bounds");
				}

				++foundPartitions;
				if (foundPartitions <= kMaxDisplayedPartitions) {
					context.write("  candidate partition ");
					decimal(context, sectorIndex * 4U + pos / 128U + 1U);
					context.write("  LBA ");
					decimal(context, first);
					context.write("..");
					decimal(context, last);
					context.writeLine("");
				}
			}
		}

		if ((crc ^ 0xFFFFFFFFU) != expectedArrayCrc) {
			return error(context, "GPT partition array CRC32 mismatch");
		}

		if (foundPartitions == 0U) {
			return error(context, "GPT has no partitions");
		}

		if (foundPartitions > kMaxDisplayedPartitions) {
			context.write("  additional partitions: ");
			decimal(context, foundPartitions - kMaxDisplayedPartitions);
			context.writeLine("");
		}

		g_sectorCount = alternateLba + 1U; // Derived from validated GPT, not SD CSD.
		g_gptValidated = true;
		context.write("SDMMC scan: validated GPT (header+entries CRC32), sectors=");
		decimal(context, g_sectorCount);
		context.write("; partitions=");
		decimal(context, foundPartitions);
		context.write("; header CRC=");
		hex32(context, expectedHeaderCrc);
		context.writeLine("");
		return true;
	}

} // namespace

void sdmmcInvalidateValidatedDisk() {
	g_gptValidated = false;
	g_sectorCount = 0;
}

bool scanSdmmc(lexxos::os::AppContext& context) {
	sdmmcInvalidateValidatedDisk();
	if (!sdmmcReady()) {
		return error(context, "run 'sdmmc probe' first");
	}

	if (!validateGpt(context)) {
		return false;
	}

	if (!g_attached) {
		if (context.devices().blockDevice("sd0") != nullptr) {
			return error(context, "sd0 already registered by another driver");
		}

		if (!context.devices().registerBlockDevice("sd0", "RK3506 SDMMC PIO read-only (GPT)", g_blockDevice)) {
			return error(context, "DeviceManager: no free block-device slot");
		}

		g_attached = true;
	}

	if (!context.vfs().registerBlockDevice("sd0", &g_blockDevice)) {
		return error(context, "VFS: no free block-device slot");
	}

	context.writeLine("SDMMC scan: /dev/sd0 registered READ ONLY (no filesystem mount)");
	return true;
}

} // namespace lexxos::platform::rk3506
