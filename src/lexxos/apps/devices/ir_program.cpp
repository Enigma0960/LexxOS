#include <lexxos/apps/devices/ir_program.h>

#include <cstdint>
#include <limits>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/infrared_transmitter.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr std::uint32_t samsungTvPowerCode = 0xE0E040BF;
	constexpr std::uint32_t samsungTvPowerOnCode = 0xE0E09966;
	constexpr std::uint32_t samsungTvPowerOffCode = 0xE0E019E6;

	constexpr lexxos::program::ProgramArgumentSpec irArguments[] = {
	    {
         .name = "repeat",
         .shortName = 'r',
         .valueName = "count",
         .help = "number of frames to send",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "1",
	     },
	    {
         .name = "code",
         .shortName = 'c',
         .valueName = "hex",
         .help = "Samsung 32-bit code for samsung-code, default E0E040BF",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "E0E040BF",
	     },
	    {
         .name = "action",
         .valueName = "action",
         .help = "info, samsung-power, samsung-on, samsung-off or samsung-code",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "IR transmitter device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeIrInfo(lexxos::os::AppContext& context, lexxos::devices::InfraredTransmitter& ir) {
		context.write("ir: ");
		context.writeLine(ir.name());
		context.write("state: ");
		context.writeLine(ir.isReady() ? "ready" : "not-ready");
		context.write("pin: ");
		writeUnsigned(context, static_cast<std::uint32_t>(ir.outputPinNumber()));
		context.writeLine("");
		context.write("carrier-hz: ");
		writeUnsigned(context, ir.carrierFrequencyHz());
		context.writeLine("");
		context.write("samsung-power-code: 0x");
		writeHex32(context, samsungTvPowerCode);
		context.writeLine("");
		context.write("samsung-on-code: 0x");
		writeHex32(context, samsungTvPowerOnCode);
		context.writeLine("");
		context.write("samsung-off-code: 0x");
		writeHex32(context, samsungTvPowerOffCode);
		context.writeLine("");
	}

	[[nodiscard]] bool hexValue(char character, std::uint8_t& value) {
		if (character >= '0' && character <= '9') {
			value = static_cast<std::uint8_t>(character - '0');
			return true;
		}
		if (character >= 'a' && character <= 'f') {
			value = static_cast<std::uint8_t>(character - 'a' + 10);
			return true;
		}
		if (character >= 'A' && character <= 'F') {
			value = static_cast<std::uint8_t>(character - 'A' + 10);
			return true;
		}
		return false;
	}

	[[nodiscard]] bool parseHex32(std::string_view text, std::uint32_t& value) {
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			text.remove_prefix(2);
		}
		if (text.empty() || text.size() > 8) {
			return false;
		}
		std::uint32_t result = 0;
		for (char const character : text) {
			std::uint8_t digit = 0;
			if (!hexValue(character, digit) || result > ((std::numeric_limits<std::uint32_t>::max() - digit) >> 4U)) {
				return false;
			}
			result = static_cast<std::uint32_t>((result << 4U) | digit);
		}
		value = result;
		return true;
	}

} // namespace

lexxos::program::ProgramDescriptor IrProgram::descriptor() {
	return {
	    .name = "ir",
	    .help = "show or test the primary IR transmitter",
	    .description = "Shows the registered IR transmitter or sends the Cardputer-Adv Samsung TV power test frame.",
	    .usage = "ir [info|samsung-power|samsung-on|samsung-off|samsung-code] [-r count] [-c hex] [name]",
	    .arguments = argumentList(irArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int IrProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positionalOr("action", "info");
	auto const requestedName = arguments.positional("name");
	auto* ir = !requestedName.empty() ? context.devices().infraredDevice(requestedName) : context.devices().primaryInfraredDevice();
	if (ir == nullptr) {
		context.writeErrorLine("ir: transmitter is not registered");
		return 1;
	}
	if (!ir->isReady()) {
		context.writeErrorLine("ir: transmitter is not ready");
		return 1;
	}

	if (action == "info") {
		writeIrInfo(context, *ir);
		return 0;
	}
	if (action != "samsung-power" && action != "samsung-on" && action != "samsung-off" && action != "samsung-code") {
		context.writeErrorLine("ir: supported actions are info, samsung-power, samsung-on, samsung-off and samsung-code");
		return 1;
	}

	std::uint32_t repeats = 1;
	if (!parseUnsigned32(arguments.valueOr("repeat", "1"), repeats) || repeats == 0 || repeats > 10) {
		context.writeErrorLine("ir: invalid repeat count");
		return 1;
	}
	std::uint32_t code = samsungTvPowerCode;
	if (action == "samsung-on") {
		code = samsungTvPowerOnCode;
	} else if (action == "samsung-off") {
		code = samsungTvPowerOffCode;
	} else if (action == "samsung-code" && !parseHex32(arguments.valueOr("code", "E0E040BF"), code)) {
		context.writeErrorLine("ir: invalid Samsung code");
		return 1;
	}

	auto lock = context.devices().lockInfraredDevice(ir->name(), lexxos::devices::DeviceResourcePriority::Interactive, "ir");
	if (!lock) {
		context.writeErrorLine("ir: transmitter is busy");
		return 1;
	}

	context.write("ir: sending ");
	context.write(action);
	context.write(" code=0x");
	writeHex32(context, code);
	context.write(" repeat=");
	writeUnsigned(context, repeats);
	context.writeLine("");
	if (!ir->transmitSamsung(code, static_cast<std::uint8_t>(repeats))) {
		context.writeErrorLine("ir: transmit failed");
		return 1;
	}
	context.writeLine("ir: sent");
	return 0;
}

} // namespace lexxos::apps::device_programs
