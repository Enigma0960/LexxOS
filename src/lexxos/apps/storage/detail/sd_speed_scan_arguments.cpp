#include <lexxos/apps/storage/detail/sd_speed_scan_arguments.h>

#include <initializer_list>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr ProgramArgumentSpec sdSpeedScanArguments[] = {
	    {
         .name = "max-hz",
         .valueName = "max-hz",
         .help = "maximum SPI baud rate to test",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = true,
	     },
	    {
         .name = "step-hz",
         .valueName = "step-hz",
         .help = "baud rate decrement between attempts",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = true,
	     },
	    {
         .name = "blocks",
         .valueName = "blocks",
         .help = "optional 512-byte blocks per FAT file, default: 64, max: 1024",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "files",
         .valueName = "files",
         .help = "optional FAT file count, default: 8, max: 64",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "scope",
         .valueName = "read|rw",
         .help = "optional probe scope: read-only raw scan or explicit raw+FAT write/read, default: read",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "batch",
         .valueName = "batch=1|2",
         .help = "optional raw read batch size; 1 uses CMD17, 2 uses CMD18",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "start",
         .valueName = "start=<block>",
         .help = "optional first raw block for diagnostics",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "dma",
         .valueName = "dma=on|off",
         .help = "optional raw payload transfer mode; off forces blocking SPI instead of DMA",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "fast-write",
         .valueName = "fast-write=on|off",
         .help = "optional high-speed FAT write guard override for rw scope",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	[[nodiscard]] bool parseSpeedScanScope(std::string_view text, SpeedScanScope& output) {
		if (text.empty() || text == "read" || text == "raw" || text == "ro") {
			output = SpeedScanScope::ReadOnly;
			return true;
		}
		if (text == "rw" || text == "write" || text == "full") {
			output = SpeedScanScope::ReadWrite;
			return true;
		}
		return false;
	}

	[[nodiscard]] bool splitKeyValue(std::string_view token, std::string_view& key, std::string_view& value) {
		auto const equalPosition = token.find('=');
		if (equalPosition == std::string_view::npos || equalPosition == 0 || equalPosition + 1 >= token.size()) {
			return false;
		}
		key = token.substr(0, equalPosition);
		value = token.substr(equalPosition + 1);
		return true;
	}

	[[nodiscard]] bool keyMatches(std::string_view key, std::initializer_list<std::string_view> aliases) {
		for (auto alias : aliases) {
			if (key == alias) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] std::string_view speedArgument(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view positionalName,
	    std::initializer_list<std::string_view> aliases) {
		for (std::size_t index = 0; index < arguments.positionalCount(); ++index) {
			std::string_view key{};
			std::string_view value{};
			if (splitKeyValue(arguments.positional(index), key, value) && keyMatches(key, aliases)) {
				return value;
			}
		}

		auto const positional = arguments.positional(positionalName);
		std::string_view key{};
		std::string_view value{};
		if (splitKeyValue(positional, key, value)) {
			return keyMatches(key, aliases) ? value : std::string_view{};
		}
		return positional;
	}

} // namespace

lexxos::program::ProgramArgumentList sdSpeedScanArgumentList() {
	return lexxos::apps::argumentList(sdSpeedScanArguments);
}

bool parseSdSpeedScanConfig(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    SdSpeedScanConfig& config) {
	auto const maxHzText = speedArgument(arguments, "max-hz", {"max", "max-hz", "max_hz"});
	auto const stepHzText = speedArgument(arguments, "step-hz", {"step", "step-hz", "step_hz"});
	if (!lexxos::apps::parseUnsigned32(maxHzText, config.maxHz) || config.maxHz < sdSpeedMinimumBaudRate()) {
		context.write("sd-speed-scan: max-hz must be >= ");
		lexxos::apps::writeUnsigned(context, sdSpeedMinimumBaudRate());
		context.writeLine("");
		return false;
	}
	if (!lexxos::apps::parseUnsigned32(stepHzText, config.stepHz) || config.stepHz == 0) {
		context.writeLine("sd-speed-scan: step-hz must be > 0");
		return false;
	}

	if (!parseOptionalCount(
	        context,
	        speedArgument(arguments, "blocks", {"blocks", "blocks-per-file", "blocks_per_file"}),
	        sdSpeedDefaultBlocksPerFile,
	        sdSpeedMaxBlocksPerFile,
	        "sd-speed-scan blocks",
	        config.blocksPerFile)) {
		return false;
	}

	if (!parseOptionalCount(
	        context,
	        speedArgument(arguments, "files", {"files", "file-count", "file_count"}),
	        sdSpeedDefaultFiles,
	        sdSpeedMaxFiles,
	        "sd-speed-scan files",
	        config.files)) {
		return false;
	}

	if (!parseSpeedScanScope(speedArgument(arguments, "scope", {"scope", "mode"}), config.scope)) {
		writeSdSpeedScanUsage(context);
		return false;
	}

	if (!parseOptionalCount(
	        context,
	        speedArgument(arguments, "batch", {"batch", "raw-batch", "raw_batch"}),
	        sdSpeedDefaultRawBatchBlocks,
	        sdSpeedMaxRawBatchBlocks,
	        "sd-speed-scan batch",
	        config.rawBatchBlocks)) {
		return false;
	}

	config.dmaTransfersEnabled = lexxos::devices::sd_card::dmaTransfersEnabled();
	auto const dmaText = speedArgument(arguments, "dma", {"dma", "sd-dma", "sd_dma"});
	if (!dmaText.empty() && !parseOnOff(dmaText, config.dmaTransfersEnabled)) {
		context.writeLine("sd-speed-scan: dma must be on or off");
		return false;
	}

	config.highSpeedWritesEnabled = config.scope == SpeedScanScope::ReadWrite;
	auto const fastWriteText = speedArgument(arguments, "fast-write", {"fast-write", "fast_write", "write", "writes"});
	if (!fastWriteText.empty() && !parseOnOff(fastWriteText, config.highSpeedWritesEnabled)) {
		context.writeLine("sd-speed-scan: fast-write must be on or off");
		return false;
	}

	return true;
}

bool resolveSdSpeedScanRawStart(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    std::uint64_t deviceBlockCount,
    SdSpeedScanConfig& config) {
	auto const rawStartText = speedArgument(arguments, "start", {"start", "raw-start", "raw_start"});
	if (!rawStartText.empty()) {
		if (!lexxos::apps::parseUnsigned64(rawStartText, config.rawStartBlock)) {
			context.writeLine("sd-speed-scan: invalid raw start block");
			return false;
		}
	} else if (deviceBlockCount > static_cast<std::uint64_t>(rawBlockCount(config)) + 2048ULL) {
		config.rawStartBlock = deviceBlockCount / 2ULL;
	}

	if (config.rawStartBlock >= deviceBlockCount || config.rawStartBlock + rawBlockCount(config) > deviceBlockCount) {
		context.writeLine("sd-speed-scan: raw start/count is outside device range");
		return false;
	}
	return true;
}

void writeSdSpeedScanUsage(lexxos::os::AppContext& context) {
	context.writeLine(
	    "usage: sd-speed-scan <max-hz> <step-hz> [blocks-per-file] [files] [read|rw] [batch=1|2] [start=<block>] [dma=on|off] [fast-write=on|off]");
}

} // namespace lexxos::apps::storage_programs::detail
