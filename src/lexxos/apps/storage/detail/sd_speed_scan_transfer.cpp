#include <lexxos/apps/storage/detail/sd_speed_scan_transfer.h>

#include <array>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_diagnostics_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs::detail {

bool setSpeedScanTransfer(
    lexxos::os::AppContext& context,
    lexxos::devices::SpiClockMode mode,
    std::uint32_t baudRate,
    char const* phase) {
	if (!lexxos::devices::sd_card::setSpiClockMode(mode)) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.writeLine(" set mode failed");
		printSdLastDiagnostic(context);
		return false;
	}
	if (!lexxos::devices::sd_card::setTransferBaudRate(baudRate)) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.writeLine(" set baud failed");
		printSdLastDiagnostic(context);
		return false;
	}
	return true;
}

bool readBlocksAtSettings(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    lexxos::devices::SpiClockMode mode,
    std::uint32_t expectedBaudRate,
    std::uint64_t firstBlock,
    std::uint32_t blockCount,
    std::uint8_t* output,
    char const* phase) {
	if (!setSpeedScanTransfer(context, mode, expectedBaudRate, phase)) {
		return false;
	}
	if (!device.readBlocks(firstBlock, output, blockCount)) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.write(" read failed at block ");
		lexxos::apps::writeUnsigned64(context, firstBlock);
		if (blockCount > 1) {
			context.write(" count=");
			lexxos::apps::writeUnsigned(context, blockCount);
		}
		context.writeLine("");
		printSdLastDiagnostic(context);
		return false;
	}
	std::uint32_t const activeBaudRate = lexxos::devices::sd_card::activeBaudRate();
	if (activeBaudRate != expectedBaudRate) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.write(" fallback at block ");
		lexxos::apps::writeUnsigned64(context, firstBlock);
		context.write(", active=");
		lexxos::apps::writeUnsigned(context, activeBaudRate);
		context.writeLine(" Hz");
		printSdLastDiagnostic(context);
		return false;
	}
	return true;
}

bool recoverSpeedScanReference(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    char const* phase) {
	lexxos::devices::sd_card::setReadCrcValidation(false);
	lexxos::devices::sd_card::setMultiBlockRead(false);

	if (!setSpeedScanTransfer(context, sdSpeedReferenceMode, sdSpeedReferenceBaudRate(), phase)) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.writeLine(" reinitializing SD card");
		if (!lexxos::devices::sd_card::initialize(context.platform())) {
			context.write("sd-speed-scan: ");
			context.write(phase);
			context.write(" reinit failed, state=");
			context.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
			return false;
		}
		lexxos::devices::sd_card::setReadCrcValidation(false);
		lexxos::devices::sd_card::setMultiBlockRead(false);
		if (!setSpeedScanTransfer(context, sdSpeedReferenceMode, sdSpeedReferenceBaudRate(), phase)) {
			return false;
		}
	}

	std::array<std::uint8_t, storageBlockSize> probe{};
	if (device.readBlocks(0, probe.data(), 1)) {
		return true;
	}

	context.write("sd-speed-scan: ");
	context.write(phase);
	context.writeLine(" read0 failed, reinitializing SD card");
	if (!lexxos::devices::sd_card::initialize(context.platform())) {
		context.write("sd-speed-scan: ");
		context.write(phase);
		context.write(" reinit failed, state=");
		context.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
		return false;
	}
	lexxos::devices::sd_card::setReadCrcValidation(false);
	lexxos::devices::sd_card::setMultiBlockRead(false);
	if (!setSpeedScanTransfer(context, sdSpeedReferenceMode, sdSpeedReferenceBaudRate(), phase)) {
		return false;
	}
	return device.readBlocks(0, probe.data(), 1);
}

bool cleanupSpeedFilesInReferenceMode(lexxos::os::AppContext& context, std::uint32_t files) {
	if (!setSpeedScanTransfer(context, sdSpeedReferenceMode, sdSpeedReferenceBaudRate(), "cleanup")) {
		return false;
	}
	cleanupSpeedFiles(files);
	return true;
}

} // namespace lexxos::apps::storage_programs::detail
