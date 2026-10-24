#include <lexxos/apps/storage/detail/sd_speed_scan_report.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	[[nodiscard]] constexpr char const* scopeName(SpeedScanScope scope) {
		return scope == SpeedScanScope::ReadOnly ? "read" : "rw";
	}

} // namespace

void writeSdSpeedScanConfiguration(
    lexxos::os::AppContext& context,
    SdSpeedScanConfig const& config,
    lexxos::devices::SpiClockMode targetMode) {
	context.write("sd-speed-scan: max=");
	lexxos::apps::writeUnsigned(context, config.maxHz);
	context.write(" Hz step=");
	lexxos::apps::writeUnsigned(context, config.stepHz);
	context.write(" Hz blocks/file=");
	lexxos::apps::writeUnsigned(context, config.blocksPerFile);
	context.write(" files=");
	lexxos::apps::writeUnsigned(context, config.files);
	context.write(" scope=");
	context.write(scopeName(config.scope));
	context.write(" target-mode=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(targetMode));
	context.write(" reference=");
	lexxos::apps::writeUnsigned(context, sdSpeedReferenceBaudRate());
	context.write(" Hz mode=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(sdSpeedReferenceMode));
	context.write(" raw-start=");
	lexxos::apps::writeUnsigned64(context, config.rawStartBlock);
	context.write(" batch=");
	lexxos::apps::writeUnsigned(context, config.rawBatchBlocks);
	context.write(" dma=");
	context.write(config.dmaTransfersEnabled ? "on" : "off");
	context.write(" fast-write=");
	context.write(config.highSpeedWritesEnabled ? "on" : "off");
	context.writeLine("");
	context.write("sd-speed-scan: crc=off cmd18=");
	context.writeLine(config.rawBatchBlocks > 1 ? "on for raw target reads" : "off for raw target reads");
}

void writeSdSpeedRawProbeReport(
    lexxos::os::AppContext& context,
    std::uint64_t bytesTotal,
    std::uint32_t elapsedMs,
    std::uint32_t referenceHash,
    std::uint32_t hashA,
    std::uint32_t hashB) {
	context.write("sd-speed-scan: raw target read ");
	lexxos::apps::writeUnsigned64(context, bytesTotal);
	context.write(" bytes in ");
	lexxos::apps::writeUnsigned(context, elapsedMs);
	context.write(" ms, ");
	writeThroughput(context, bytesTotal, elapsedMs);
	context.write(" ref=0x");
	lexxos::apps::writeHex32(context, referenceHash);
	context.write(" hash1=0x");
	lexxos::apps::writeHex32(context, hashA);
	context.write(" hash2=0x");
	lexxos::apps::writeHex32(context, hashB);
	context.writeLine("");
}

void writeSdSpeedFatProbeReport(
    lexxos::os::AppContext& context,
    char const* phase,
    std::uint64_t bytes,
    std::uint32_t elapsedMs) {
	context.write("sd-speed-scan: FAT ");
	context.write(phase);
	context.write(" ");
	lexxos::apps::writeUnsigned64(context, bytes);
	context.write(" bytes in ");
	lexxos::apps::writeUnsigned(context, elapsedMs);
	context.write(" ms, ");
	writeThroughput(context, bytes, elapsedMs);
	context.writeLine("");
}

void writeSdSpeedActiveBaud(lexxos::os::AppContext& context, char const* phase, std::uint32_t activeBaudRate) {
	context.write("sd-speed-scan: active after ");
	context.write(phase);
	context.write(" ");
	lexxos::apps::writeUnsigned(context, activeBaudRate);
	context.writeLine(" Hz");
}

void writeSdSpeedResult(lexxos::os::AppContext& context, std::uint32_t baudRate, bool stable) {
	context.write("sd-speed-scan: ");
	lexxos::apps::writeUnsigned(context, baudRate);
	context.write(" Hz result=");
	context.writeLine(stable ? "stable" : "failed");
}

void writeSdSpeedSelected(lexxos::os::AppContext& context, std::uint32_t baudRate) {
	context.write("sd-speed-scan: selected ");
	lexxos::apps::writeUnsigned(context, baudRate);
	context.writeLine(" Hz");
}

} // namespace lexxos::apps::storage_programs::detail
