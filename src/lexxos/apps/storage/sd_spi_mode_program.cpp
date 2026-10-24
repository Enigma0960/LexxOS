#include <lexxos/apps/storage/sd_spi_mode_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
         .name = "mode",
         .valueName = "mode",
         .help = "SPI clock mode 0..3; without argument prints current mode",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor SdSpiModeProgram::descriptor() {
	return {
	    .name = "sd-spi-mode",
	    .help = "show or set SD SPI clock mode",
	    .description = "Shows or changes the RP2040 SPI clock mode used for the SD card. The stable profile uses mode 0; modes 1..3 are disabled unless the source is rebuilt for SD speed experiments.",
	    .usage = "sd-spi-mode [0|1|2|3]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdSpiModeProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const requestedText = parsedArguments.positional("mode");
	if (requestedText.empty()) {
		context.write("sd-spi-mode: active ");
		writeUnsigned(context, static_cast<std::uint32_t>(lexxos::devices::sd_card::spiClockMode()));
		context.writeLine("");
		return 0;
	}

	lexxos::devices::SpiClockMode mode = lexxos::devices::SpiClockMode::Mode0;
	if (!detail::parseSpiMode(requestedText, mode)) {
		context.writeLine("usage: sd-spi-mode [0|1|2|3]");
		return 1;
	}

#if !defined(LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS)
	if (mode != lexxos::devices::SpiClockMode::Mode0) {
		context.writeLine("sd-spi-mode: stable build allows only mode 0");
		return 1;
	}
#endif

	if (!lexxos::devices::sd_card::setSpiClockMode(mode)) {
		context.writeLine("sd-spi-mode: set failed");
		return 1;
	}

	context.write("sd-spi-mode: active ");
	writeUnsigned(context, static_cast<std::uint32_t>(lexxos::devices::sd_card::spiClockMode()));
	context.writeLine("");
	return 0;
}

} // namespace lexxos::apps::storage_programs
