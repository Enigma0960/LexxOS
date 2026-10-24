#include <lexxos/apps/i2c_scan_program.h>

#include <cstddef>
#include <cstdint>

#include <lexxos/apps/detail/i2c_program_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::i2c_programs {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentList;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;

namespace {

	template <std::size_t Size>
	constexpr ProgramArgumentList argumentList(ProgramArgumentSpec const (&arguments)[Size]) {
		return ProgramArgumentList{
		    .data = arguments,
		    .size = Size,
		};
	}

	constexpr ProgramArgumentSpec i2cScanArguments[] = {
	    {
         .name = "bus-name",
         .valueName = "name",
         .help = "optional I2C bus name positional alias",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "bus",
         .shortName = 'b',
         .valueName = "name",
         .help = "I2C bus name",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "debug",
	     },
	    {
         .name = "from",
         .valueName = "addr",
         .help = "first 7-bit address, decimal or hex",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0x08",
	     },
	    {
         .name = "to",
         .valueName = "addr",
         .help = "last 7-bit address, decimal or hex",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0x77",
	     },
	    {
         .name = "quiet",
         .shortName = 'q',
         .valueName = "",
         .help = "print detected addresses as a plain list",
         .kind = ProgramArgumentKind::Flag,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

ProgramDescriptor I2cScanProgram::descriptor() {
	return {
	    .name = "i2c-scan",
	    .help = "scan I2C bus",
	    .description = "Scans a registered I2C bus and prints an i2cdetect-style address table to stdout. Use shell redirection to save the report to /tmp.",
	    .usage = "i2c-scan [<bus-name>] [--bus <name>] [--from <addr>] [--to <addr>] [--quiet]",
	    .arguments = argumentList(i2cScanArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int I2cScanProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const busName = arguments.has("bus") ? arguments.valueOr("bus", "debug") : arguments.positionalOr("bus-name", "debug");
	std::uint32_t from = 0x08;
	std::uint32_t to = 0x77;
	if (!detail::parseUnsigned(arguments.valueOr("from", "0x08"), from) || from > 0x7F) {
		context.writeErrorLine("i2c-scan: invalid --from address");
		return 2;
	}
	if (!detail::parseUnsigned(arguments.valueOr("to", "0x77"), to) || to > 0x7F) {
		context.writeErrorLine("i2c-scan: invalid --to address");
		return 2;
	}
	if (from > to) {
		context.writeErrorLine("i2c-scan: --from must be less than or equal to --to");
		return 2;
	}

	auto* bus = context.devices().i2cBus(busName);
	if (bus == nullptr) {
		context.writeError("i2c-scan: unknown I2C bus: ");
		context.writeErrorLine(busName);
		context.writeErrorLine("available buses:");
		detail::writeI2cBusList(context, true);
		return 1;
	}

	auto busLock = context.devices().lockI2cBus(busName, lexxos::devices::DeviceResourcePriority::Interactive, "i2c-scan");
	if (!busLock) {
		context.writeError("i2c-scan: I2C bus is busy: ");
		context.writeErrorLine(busName);
		return 1;
	}

	if (!bus->isReady()) {
		context.writeError("i2c-scan: I2C bus is not ready: ");
		context.writeErrorLine(busName);
		return 1;
	}

	lexxos::devices::I2cBusTransaction transaction{*bus};
	if (!transaction) {
		context.writeError("i2c-scan: failed to start I2C transaction: ");
		context.writeErrorLine(busName);
		return 1;
	}

	bool const quiet = arguments.has("quiet");
	std::uint32_t found = 0;
	if (quiet) {
		for (std::uint32_t address = from; address <= to; ++address) {
			if (!bus->probe(static_cast<std::uint8_t>(address))) {
				continue;
			}
			++found;
			detail::writeHex8(context, static_cast<std::uint8_t>(address));
			context.writeLine("");
		}
	} else {
		context.write("scan I2C bus '");
		context.write(busName);
		context.write("' from ");
		detail::writeHex8(context, static_cast<std::uint8_t>(from));
		context.write(" to ");
		detail::writeHex8(context, static_cast<std::uint8_t>(to));
		context.writeLine("");

		found = detail::writeI2cAddressTable(context,
		    *bus,
		    static_cast<std::uint8_t>(from),
		    static_cast<std::uint8_t>(to));

		context.write("found: ");
		detail::writeUnsigned(context, found);
		context.writeLine("");
	}
	return 0;
}

} // namespace lexxos::apps::i2c_programs
