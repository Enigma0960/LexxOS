#include <lexxos/apps/storage/detail/sd_speed_scan_fat_probe.h>

#include <array>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::storage_programs::detail {

bool runSpeedFatWriteProbe(
    lexxos::os::AppContext& context,
    std::uint32_t blocksPerFile,
    std::uint32_t files,
    std::uint32_t& elapsedMsOut) {
	std::array<char, storageBlockSize> writeBuffer{};
	std::uint32_t const startedMs = lexxos::apps::uptimeMs(context);

	cleanupSpeedFiles(files);
	for (std::uint32_t fileIndex = 0; fileIndex < files; ++fileIndex) {
		std::array<char, lexxos::vfs::maxPathLength> path{};
		if (!makeSpeedPath(fileIndex, path)) {
			context.writeLine("sd-speed-scan: path too long");
			return false;
		}

		lexxos::vfs::FileHandle output;
		auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::CreateTruncateWrite, output);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			context.write("sd-speed-scan: open write failed: ");
			context.write(path.data());
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(openStatus));
			context.writeLine("  hint: run 'ls /mnt/sd' and remove old LXSPD*.BIN files if they exist or are read-only");
			return false;
		}

		for (std::uint32_t block = 0; block < blocksPerFile; ++block) {
			fillPattern(writeBuffer, 73U + fileIndex * 17U, static_cast<std::uint64_t>(block) * storageBlockSize);
			auto const writeStatus = output.write(std::string_view{writeBuffer.data(), writeBuffer.size()});
			if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
				output.close();
				context.write("sd-speed-scan: write failed: ");
				context.writeLine(lexxos::vfs::toString(writeStatus));
				return false;
			}
		}
		output.close();
	}

	auto const syncStatus = lexxos::vfs::sync();
	if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
		context.write("sd-speed-scan: sync failed: ");
		context.writeLine(lexxos::vfs::toString(syncStatus));
		return false;
	}

	elapsedMsOut = elapsedMs(startedMs, lexxos::apps::uptimeMs(context));
	return true;
}

bool runSpeedFatReadProbe(
    lexxos::os::AppContext& context,
    std::uint32_t blocksPerFile,
    std::uint32_t files,
    std::uint32_t& elapsedMsOut) {
	std::array<char, storageBlockSize> readBuffer{};
	std::uint32_t const startedMs = lexxos::apps::uptimeMs(context);

	for (std::uint32_t fileIndex = 0; fileIndex < files; ++fileIndex) {
		std::array<char, lexxos::vfs::maxPathLength> path{};
		if (!makeSpeedPath(fileIndex, path)) {
			context.writeLine("sd-speed-scan: path too long");
			return false;
		}

		lexxos::vfs::FileHandle input;
		auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::Read, input);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			context.write("sd-speed-scan: open read failed: ");
			context.write(path.data());
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(openStatus));
			return false;
		}

		std::uint64_t offset = 0;
		for (std::uint32_t block = 0; block < blocksPerFile; ++block) {
			std::size_t bytesRead = 0;
			auto const readStatus = input.read(readBuffer.data(), readBuffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok || bytesRead != readBuffer.size()) {
				input.close();
				context.write("sd-speed-scan: read failed: ");
				if (readStatus == lexxos::vfs::ReadStatus::Ok) {
					context.writeLine("short read");
				} else {
					context.writeLine(lexxos::vfs::toString(readStatus));
				}
				return false;
			}
			if (!verifyPattern(readBuffer, bytesRead, 73U + fileIndex * 17U, offset)) {
				input.close();
				context.write("sd-speed-scan: verify failed: ");
				context.writeLine(path.data());
				return false;
			}
			offset += bytesRead;
		}
		input.close();
	}

	elapsedMsOut = elapsedMs(startedMs, lexxos::apps::uptimeMs(context));
	return true;
}

} // namespace lexxos::apps::storage_programs::detail
