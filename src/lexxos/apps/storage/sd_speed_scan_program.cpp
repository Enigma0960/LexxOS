#include <lexxos/apps/storage/sd_speed_scan_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_arguments.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_fat_probe.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_raw_probe.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_report.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_settings_guard.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_transfer.h>
#include <lexxos/apps/storage/detail/sd_speed_scan_types.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor SdSpeedScanProgram::descriptor() {
	return {
	    .name = "sd-speed-scan",
	    .help = "find stable SD SPI baud rate",
	    .description = "Scans SD SPI transfer rates. Supports positional arguments and key=value aliases such as max=25000000 step=1000000 blocks=16 files=4.",
	    .usage = "sd-speed-scan <max-hz> <step-hz> [blocks-per-file] [files] [read|rw] [batch=1|2] [start=<block>] [dma=on|off] [fast-write=on|off]",
	    .arguments = detail::sdSpeedScanArgumentList(),
	    .handler = run,
	};
}

int SdSpeedScanProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if !defined(LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS)
	(void) arguments;
	context.writeLine("sd-speed-scan: disabled in stable SD build");
	context.writeLine("sd-speed-scan: rebuild with LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS=1 to continue speed diagnostics");
	return 1;
#else
	if (!ensureStorageReady(context)) {
		return 1;
	}

	detail::SdSpeedScanConfig config{};
	if (!detail::parseSdSpeedScanConfig(context, arguments, config)) {
		return 1;
	}

	auto* device = sdBlockDevice(context);
	if (device == nullptr || !device->isReady()) {
		context.writeLine("sd-speed-scan: sd0 is not ready");
		return 1;
	}

	if (!detail::resolveSdSpeedScanRawStart(context, arguments, device->blockCount(), config)) {
		return 1;
	}

	std::uint64_t const fatBytes = detail::fatByteCount(config);
	lexxos::devices::SpiClockMode const targetMode = lexxos::devices::sd_card::spiClockMode();

	detail::SdSpeedScanSettingsGuard settingsGuard;
	lexxos::devices::sd_card::setReadCrcValidation(false);
	lexxos::devices::sd_card::setMultiBlockRead(config.rawBatchBlocks > 1);
	lexxos::devices::sd_card::setDmaTransfersEnabled(config.dmaTransfersEnabled);
	lexxos::devices::sd_card::setHighSpeedWritesEnabled(config.highSpeedWritesEnabled);

	detail::writeSdSpeedScanConfiguration(context, config, targetMode);
	if (!detail::recoverSpeedScanReference(context, *device, "prepare")) {
		return 1;
	}

	if (config.scope == detail::SpeedScanScope::ReadWrite) {
		context.writeLine("sd-speed-scan: rw scope writes temporary files to /mnt/sd; use only on a backed-up card");
		if (config.highSpeedWritesEnabled) {
			context.writeLine("sd-speed-scan: high-speed FAT writes are temporarily enabled for this scan");
		} else {
			context.writeLine("sd-speed-scan: high-speed FAT writes are guarded; pass fast-write=on to test target write speed");
		}
		if (!detail::cleanupSpeedFilesInReferenceMode(context, config.files)) {
			return 1;
		}
	}

	bool testedMinimum = false;
	for (std::uint32_t baudRate = config.maxHz;;) {
		if (baudRate <= detail::sdSpeedMinimumBaudRate()) {
			baudRate = detail::sdSpeedMinimumBaudRate();
			testedMinimum = true;
		}

		context.write("sd-speed-scan: testing ");
		writeUnsigned(context, baudRate);
		context.writeLine(" Hz");

		if (detail::setSpeedScanTransfer(context, targetMode, baudRate, "target")) {
			bool rawStable = false;
			bool rawOk = false;
			{
				auto storageLock = lockSdBlockDevice(context, "sd-speed-scan");
				if (!storageLock) {
					return 1;
				}
				rawOk = detail::runSpeedRawProbe(
				    context,
				    *device,
				    targetMode,
				    baudRate,
				    config.rawStartBlock,
				    detail::rawBlockCount(config),
				    config.rawBatchBlocks,
				    rawStable);
			}

			std::uint32_t const activeAfterRaw = lexxos::devices::sd_card::activeBaudRate();
			detail::writeSdSpeedActiveBaud(context, "raw", activeAfterRaw);

			bool fatOk = false;
			std::uint32_t writeElapsedMs = 0;
			std::uint32_t readElapsedMs = 0;
			if (rawOk && rawStable && activeAfterRaw == baudRate) {
				if (config.scope == detail::SpeedScanScope::ReadOnly) {
					fatOk = true;
				} else if (!detail::setSpeedScanTransfer(context, targetMode, baudRate, "FAT")) {
					context.writeLine("sd-speed-scan: set transfer failed before FAT probe");
				} else {
					fatOk = detail::runSpeedFatWriteProbe(context, config.blocksPerFile, config.files, writeElapsedMs)
					    && detail::runSpeedFatReadProbe(context, config.blocksPerFile, config.files, readElapsedMs);
				}

				if (writeElapsedMs != 0) {
					detail::writeSdSpeedFatProbeReport(context, "write", fatBytes, writeElapsedMs);
				}
				if (readElapsedMs != 0) {
					detail::writeSdSpeedFatProbeReport(context, "read/verify", fatBytes, readElapsedMs);
				}
			}

			std::uint32_t const activeAfterFat = lexxos::devices::sd_card::activeBaudRate();
			detail::writeSdSpeedActiveBaud(context, "FAT", activeAfterFat);

			if (config.scope == detail::SpeedScanScope::ReadWrite) {
				(void) detail::cleanupSpeedFilesInReferenceMode(context, config.files);
			}

			bool const stable = rawOk && rawStable && fatOk && activeAfterRaw == baudRate && (config.scope == detail::SpeedScanScope::ReadOnly || activeAfterFat == baudRate);
			detail::writeSdSpeedResult(context, baudRate, stable);
			if (stable) {
				detail::writeSdSpeedSelected(context, baudRate);
				return 0;
			}
		}

		(void) detail::recoverSpeedScanReference(context, *device, "recover");

		if (testedMinimum || baudRate <= config.stepHz) {
			break;
		}
		baudRate -= config.stepHz;
	}

	if (config.scope == detail::SpeedScanScope::ReadWrite) {
		(void) detail::cleanupSpeedFilesInReferenceMode(context, config.files);
	}

	context.writeLine("sd-speed-scan: no stable speed found");
	return 1;
#endif
}

} // namespace lexxos::apps::storage_programs
