#include <lexxos/apps/storage/detail/sd_speed_scan_raw_probe.h>

#include <algorithm>
#include <array>
#include <cstring>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_diagnostics_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_report.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_transfer.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	void printFirstDifference(
	    lexxos::os::AppContext& context,
	    std::uint8_t const* expected,
	    std::uint8_t const* actual,
	    std::size_t size) {
		for (std::size_t offset = 0; offset < size; ++offset) {
			if (expected[offset] != actual[offset]) {
				context.write(" offset=");
				lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(offset));
				context.write(" expected=0x");
				lexxos::apps::writeHexByte(context, expected[offset]);
				context.write(" actual=0x");
				lexxos::apps::writeHexByte(context, actual[offset]);
				return;
			}
		}
	}

} // namespace

bool runSpeedRawProbe(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    lexxos::devices::SpiClockMode targetMode,
    std::uint32_t expectedBaudRate,
    std::uint64_t firstBlock,
    std::uint32_t blocks,
    std::uint32_t rawBatchBlocks,
    bool& stable) {
	stable = false;

	std::array<std::uint8_t, storageBlockSize * sdSpeedMaxRawBatchBlocks> referenceBlocks{};
	std::array<std::uint8_t, storageBlockSize * sdSpeedMaxRawBatchBlocks> blockA{};
	std::array<std::uint8_t, storageBlockSize * sdSpeedMaxRawBatchBlocks> blockB{};
	std::uint32_t referenceHash = 2166136261U;
	std::uint32_t hashA = 2166136261U;
	std::uint32_t hashB = 2166136261U;
	std::uint32_t targetElapsedMs = 0;
	std::uint32_t const clampedBatchBlocks = std::clamp<std::uint32_t>(rawBatchBlocks, 1, sdSpeedMaxRawBatchBlocks);
	bool const useMultiBlockTargetRead = clampedBatchBlocks > 1;

	for (std::uint32_t block = 0; block < blocks;) {
		std::uint32_t const batchBlocks = std::min<std::uint32_t>(clampedBatchBlocks, blocks - block);
		std::size_t const batchBytes = static_cast<std::size_t>(batchBlocks) * storageBlockSize;
		std::uint64_t const blockNumber = firstBlock + block;

		lexxos::devices::sd_card::setMultiBlockRead(false);
		if (!readBlocksAtSettings(
		        context,
		        device,
		        sdSpeedReferenceMode,
		        sdSpeedReferenceBaudRate(),
		        blockNumber,
		        batchBlocks,
		        referenceBlocks.data(),
		        "reference")) {
			return false;
		}

		std::uint32_t const targetStartedMs = lexxos::apps::uptimeMs(context);
		lexxos::devices::sd_card::setMultiBlockRead(useMultiBlockTargetRead);
		if (!readBlocksAtSettings(context, device, targetMode, expectedBaudRate, blockNumber, batchBlocks, blockA.data(), "raw")) {
			return false;
		}
		if (!readBlocksAtSettings(context, device, targetMode, expectedBaudRate, blockNumber, batchBlocks, blockB.data(), "raw")) {
			return false;
		}

		targetElapsedMs += elapsedMs(targetStartedMs, lexxos::apps::uptimeMs(context));
		referenceHash = hashBytes(referenceHash, referenceBlocks.data(), batchBytes);
		hashA = hashBytes(hashA, blockA.data(), batchBytes);
		hashB = hashBytes(hashB, blockB.data(), batchBytes);
		if (std::memcmp(blockA.data(), blockB.data(), batchBytes) != 0) {
			context.write("sd-speed-scan: raw repeat mismatch at block ");
			lexxos::apps::writeUnsigned64(context, blockNumber);
			printFirstDifference(context, blockA.data(), blockB.data(), batchBytes);
			context.writeLine("");
			printSdLastDiagnostic(context);
			return true;
		}
		if (std::memcmp(referenceBlocks.data(), blockA.data(), batchBytes) != 0) {
			context.write("sd-speed-scan: raw reference mismatch at block ");
			lexxos::apps::writeUnsigned64(context, blockNumber);
			printFirstDifference(context, referenceBlocks.data(), blockA.data(), batchBytes);
			context.writeLine("");
			printSdLastDiagnostic(context);
			return true;
		}

		block += batchBlocks;
	}

	std::uint32_t const elapsedTotal = targetElapsedMs == 0 ? 1 : targetElapsedMs;
	std::uint64_t const bytesTotal = static_cast<std::uint64_t>(blocks) * storageBlockSize * 2ULL;
	writeSdSpeedRawProbeReport(context, bytesTotal, elapsedTotal, referenceHash, hashA, hashB);

	stable = referenceHash == hashA && hashA == hashB;
	return true;
}

} // namespace lexxos::apps::storage_programs::detail
