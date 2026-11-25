/* RK3506 stage 8f: inspect GPT partition boot records and ext2/3/4 superblocks.
 * This module NEVER modifies the card and never attempts a filesystem mount.
 * It requires sdmmc scan to validate GPT before inspecting partition contents.
 */
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::size_t kSectorSize = 512;
	constexpr std::uint32_t kMaxEntriesInspected = 16;

	std::uint16_t le16(std::uint8_t const* p) {
		return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8U);
	}

	std::uint32_t le32(std::uint8_t const* p) {
		return std::uint32_t(le16(p)) | (std::uint32_t(le16(p + 2)) << 16U);
	}

	std::uint64_t le64(std::uint8_t const* p) {
		return std::uint64_t(le32(p)) | (std::uint64_t(le32(p + 4)) << 32U);
	}

	void decimal(lexxos::os::AppContext& ctx, std::uint64_t n) {
		char buffer[21]{};
		std::size_t pos = sizeof(buffer) - 1;
		do {
			buffer[--pos] = static_cast<char>('0' + n % 10U);
			n /= 10U;
		} while (n != 0);

		ctx.write(buffer + pos);
	}

	void hex32(lexxos::os::AppContext& ctx, std::uint32_t n) {
		char buffer[] = "0x00000000";
		constexpr char digits[] = "0123456789abcdef";
		for (unsigned i = 0; i < 8; ++i) {
			buffer[2 + i] = digits[(n >> (28U - 4U * i)) & 15U];
		}

		ctx.write(buffer);
	}

	void flag(lexxos::os::AppContext& ctx, char const* title, bool enabled) {
		ctx.write(title);
		ctx.write(enabled ? "yes" : "no");
	}

	bool allZero(std::uint8_t const* p, std::size_t count) {
		for (std::size_t i = 0; i < count; ++i) {
			if (p[i] != 0) {
				return false;
			}
		}

		return true;
	}

	bool fail(lexxos::os::AppContext& ctx, char const* message) {
		ctx.write("sdmount inspect: ");
		ctx.writeLine(message);
		return false;
	}

	// The magic lives 0x38 bytes into the superblock at partition offset 1024.
	// This is sector start+2 at byte 56 for 512-byte sectors.
	void describeExt(lexxos::os::AppContext& ctx, std::uint8_t const* sb) {
		std::uint32_t const incompat = le32(sb + 0x60);
		std::uint32_t const compat = le32(sb + 0x5C);
		std::uint32_t const roCompat = le32(sb + 0x64);
		std::uint32_t const logBlockSize = le32(sb + 0x18);
		std::uint32_t const inodeSize = le16(sb + 0x58);
		std::uint32_t const descSize = le16(sb + 0xFE);
		std::uint32_t const fsState = le16(sb + 0x3A);

		ctx.write("    ext superblock: ");
		if (logBlockSize <= 6U) {
			ctx.write("block-size=");
			decimal(ctx, 1024U << logBlockSize);
		} else {
			ctx.write("invalid block-size field=");
			decimal(ctx, logBlockSize);
		}

		ctx.write(" inode-size=");
		decimal(ctx, inodeSize == 0U ? 128U : inodeSize);
		ctx.write(" descriptor-size=");
		decimal(ctx, descSize);
		ctx.write(" state=");
		hex32(ctx, fsState);
		ctx.writeLine("");

		ctx.write("    features: compat=");
		hex32(ctx, compat);
		ctx.write(" incompat=");
		hex32(ctx, incompat);
		ctx.write(" ro_compat=");
		hex32(ctx, roCompat);
		ctx.writeLine("");

		ctx.write("    ");
		flag(ctx, "extents=", (incompat & 0x40U) != 0);
		ctx.write(" ");
		flag(ctx, "64bit=", (incompat & 0x80U) != 0);
		ctx.write(" ");
		flag(ctx, "metadata_csum=", (roCompat & 0x400U) != 0);
		ctx.write(" ");
		flag(ctx, "journal=", (compat & 0x04U) != 0);
		ctx.writeLine("");

		ctx.write("    group: blocks-per-group=");
		decimal(ctx, le32(sb + 0x20));
		ctx.write(" inodes-per-group=");
		decimal(ctx, le32(sb + 0x28));
		ctx.write(" blocks-low=");
		decimal(ctx, le32(sb + 0x04));
		ctx.writeLine("");
	}

	bool looksLikeFat(std::uint8_t const* boot) {
		return (boot[0] == 0xEB || boot[0] == 0xE9) && boot[510] == 0x55 && boot[511] == 0xAA && le16(boot + 11) == 512 && boot[13] != 0;
	}

} // namespace

bool inspectSdmmcPartitions(lexxos::os::AppContext& ctx) {
	auto* disk = ctx.vfs().blockDevice("sd0");
	if (disk == nullptr || !disk->isReady() || !disk->isReadOnly() || disk->blockSizeBytes() != kSectorSize) {
		return fail(ctx, "run 'sdmmc probe' and 'sdmmc scan' first");
	}

	alignas(4) std::uint8_t header[kSectorSize]{};
	alignas(4) std::uint8_t entrySector[kSectorSize]{};
	alignas(4) std::uint8_t content[kSectorSize]{};
	if (!disk->readBlocks(1, header, 1) || header[0] != 'E' || header[1] != 'F' || header[2] != 'I' || header[3] != ' ' || header[4] != 'P' || header[5] != 'A' || header[6] != 'R' || header[7] != 'T') {
		return fail(ctx, "GPT header could not be read or is invalid");
	}

	std::uint64_t const entriesStart = le64(header + 72);
	std::uint32_t const entryCount = le32(header + 80);
	std::uint32_t const entrySize = le32(header + 84);
	if (entryCount == 0 || entrySize != 128 || entriesStart < 2 || entriesStart >= disk->blockCount()) {
		return fail(ctx, "unsupported GPT entry table");
	}

	ctx.writeLine("sdmount inspect: GPT partition signatures (READ ONLY)");
	std::uint32_t printed = 0;
	std::uint32_t const inspected = entryCount < kMaxEntriesInspected ? entryCount : kMaxEntriesInspected;
	for (std::uint32_t index = 0; index < inspected; ++index) {
		std::uint64_t const entryLba = entriesStart + index / 4U;
		if (entryLba >= disk->blockCount() || !disk->readBlocks(entryLba, entrySector, 1)) {
			return fail(ctx, "GPT partition entry read failed");
		}

		auto const* entry = entrySector + (index % 4U) * 128U;
		if (allZero(entry, 16U)) {
			continue;
		}

		std::uint64_t const first = le64(entry + 32);
		std::uint64_t const last = le64(entry + 40);
		if (first > last || last >= disk->blockCount()) {
			return fail(ctx, "GPT partition entry outside disk bounds");
		}

		ctx.write("  p");
		decimal(ctx, index + 1U);
		ctx.write(" LBA=");
		decimal(ctx, first);
		ctx.write("..");
		decimal(ctx, last);
		ctx.write("  ");
		++printed;

		if (!disk->readBlocks(first, content, 1)) {
			return fail(ctx, "cannot read partition's first sector");
		}

		if (looksLikeFat(content)) {
			ctx.writeLine("FAT-like boot sector (not yet verified FAT32)");
			continue;
		}

		if (last - first < 2U || !disk->readBlocks(first + 2U, content, 1)) {
			return fail(ctx, "cannot inspect partition's possible ext superblock");
		}

		if (le16(content + 0x38U) == 0xEF53U) {
			ctx.writeLine("ext2/3/4 superblock (magic 0xef53)");
			describeExt(ctx, content);
		} else {
			ctx.writeLine("unrecognized / raw partition (no FAT or ext magic)");
		}
	}

	if (printed == 0) {
		ctx.writeLine("  no used GPT entries in inspected range");
	}

	if (entryCount > inspected) {
		ctx.write("  GPT entries scanned: ");
		decimal(ctx, inspected);
		ctx.write(" / ");
		decimal(ctx, entryCount);
		ctx.writeLine(" (initial entries only)");
	}

	ctx.writeLine("sdmount inspect: no filesystem mounted, no sectors written");
	return true;
}

} // namespace lexxos::platform::rk3506
