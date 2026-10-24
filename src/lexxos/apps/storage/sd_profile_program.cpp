#include <lexxos/apps/storage/sd_profile_program.h>

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
         .name = "profile",
         .valueName = "safe|default|performance|fast-read",
         .help = "profile to apply: safe=1MHz, default=50MHz CMD18 read, performance=60MHz",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeActiveProfile(lexxos::os::AppContext& context) {
		context.write("mode=");
		writeUnsigned(context, static_cast<std::uint32_t>(lexxos::devices::sd_card::spiClockMode()));
		context.write(" baud=");
		writeUnsigned(context, lexxos::devices::sd_card::activeBaudRate());
		context.write(" crc=");
		context.write(lexxos::devices::sd_card::readCrcValidation() ? "on" : "off");
		context.write(" multiblock=");
		context.write(lexxos::devices::sd_card::multiBlockReadEnabled() ? "on" : "off");
		context.write(" high-speed-writes=");
		context.write(lexxos::devices::sd_card::highSpeedWritesEnabled() ? "on" : "off");
		context.write(" multiwrite=");
		context.writeLine(lexxos::devices::sd_card::multiBlockWriteEnabled() ? "on" : "off");
	}

} // namespace

lexxos::program::ProgramDescriptor SdProfileProgram::descriptor() {
	return {
	    .name = "sd-profile",
	    .help = "apply known SD SPI profile",
	    .description = "Applies a known SD SPI profile. default uses the stable 50MHz CMD18 read baseline and keeps CMD25 disabled; performance uses 60MHz for explicit tests.",
	    .usage = "sd-profile [safe|default|performance|fast-read]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdProfileProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const profile = parsedArguments.positional("profile");
	if (profile.empty()) {
		context.write("sd-profile: ");
		writeActiveProfile(context);
		context.writeLine("sd-profile: available safe, default, performance, fast-read");
		return 0;
	}

	lexxos::devices::SpiClockMode mode = lexxos::devices::SpiClockMode::Mode0;
	std::uint32_t baud = detail::sdStableBaudRate();
	bool crc = false;
	bool multiBlock = true;
	bool highSpeedWrites = true;
	bool multiBlockWrite = false;
	if (profile == "safe") {
		baud = detail::sdSafeBaudRate();
		multiBlock = false;
		highSpeedWrites = false;
	} else if (profile == "default" || profile == "fast-read") {
		baud = detail::sdStableBaudRate();
	} else if (profile == "performance" || profile == "perf") {
		baud = detail::sdPerformanceBaudRate();
	} else {
		context.writeLine("usage: sd-profile [safe|default|performance|fast-read]");
		return 1;
	}

	if (!lexxos::devices::sd_card::setSpiClockMode(mode)) {
		context.writeLine("sd-profile: set mode failed");
		return 1;
	}
	lexxos::devices::sd_card::setReadCrcValidation(crc);
	lexxos::devices::sd_card::setMultiBlockRead(multiBlock);
	lexxos::devices::sd_card::setHighSpeedWritesEnabled(highSpeedWrites);
	lexxos::devices::sd_card::setMultiBlockWrite(multiBlockWrite);
	if (!lexxos::devices::sd_card::setTransferBaudRate(baud)) {
		context.writeLine("sd-profile: set baud failed");
		return 1;
	}

	context.write("sd-profile: ");
	context.write(profile);
	context.write(" ");
	writeActiveProfile(context);
	return 0;
}

} // namespace lexxos::apps::storage_programs
