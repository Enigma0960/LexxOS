#include <lexxos/apps/devices/display_text_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec displayTextArguments[] = {
	    {	     .name = "x",      .valueName = "pixels",                                         .help = "left coordinate", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned, .required = true},
	    {	     .name = "y",      .valueName = "pixels",                                          .help = "top coordinate", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned, .required = true},
	    {     .name = "scale",        .valueName = "1..8",                                      .help = "integer text scale", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned, .required = true},
	    {.name = "foreground", .valueName = "rgb565|name",                                              .help = "text color", .kind = lexxos::program::ProgramArgumentKind::Positional,   .valueType = lexxos::program::ProgramArgumentValueType::String, .required = true},
	    {.name = "background", .valueName = "rgb565|name",                                        .help = "background color", .kind = lexxos::program::ProgramArgumentKind::Positional,   .valueType = lexxos::program::ProgramArgumentValueType::String, .required = true},
	    {      .name = "text",        .valueName = "text", .help = "text to draw; additional words are appended with spaces", .kind = lexxos::program::ProgramArgumentKind::Positional,   .valueType = lexxos::program::ProgramArgumentValueType::String, .required = true},
	};

} // namespace

lexxos::program::ProgramDescriptor DisplayTextProgram::descriptor() {
	return {
	    .name = "display-text",
	    .help = "draw simple text on display",
	    .description = "Draws 5x7 uppercase ASCII text using RGB565 colors. Additional words are appended with spaces.",
	    .usage = "display-text <x> <y> <scale> <fg> <bg> <text...>",
	    .arguments = argumentList(displayTextArguments),
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayTextProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "display-text: display device is not registered"
		        : "display-text: display device is not ready");
		return 1;
	}

	// Parse position, scale and RGB565 colors.
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t scale = 0;
	std::uint16_t foreground = 0;
	std::uint16_t background = 0;
	if (!detail::parseUnsigned(arguments.positional("x"), x)
	    || !detail::parseUnsigned(arguments.positional("y"), y)
	    || !detail::parseUnsigned(arguments.positional("scale"), scale)
	    || !detail::parseRgb565Color(arguments.positional("foreground"), foreground)
	    || !detail::parseRgb565Color(arguments.positional("background"), background)) {
		context.writeErrorLine("display-text: invalid argument");
		return 1;
	}
	if (scale == 0 || scale > 8 || x > 0x7FFFFFFFU || y > 0x7FFFFFFFU) {
		context.writeErrorLine("display-text: invalid position or scale");
		return 1;
	}

	auto diagnosticLocks = detail::lockDisplayDiagnosticResources(context, *display, "display-text");
	if (!diagnosticLocks) {
		context.writeErrorLine("display-text: display or input device is busy");
		return 1;
	}

	int drawnWidth = 0;
	bool drawn = false;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		drawn = detail::drawTextLine(*display, static_cast<int>(x), static_cast<int>(y), static_cast<int>(scale),
		    foreground, background, invocation, 6, drawnWidth);
	}

	if (!drawn) {
		context.writeErrorLine("display-text: draw failed");
		return 1;
	}

	context.write("display-text: drawn width=");
	writeUnsigned(context, static_cast<std::uint32_t>(drawnWidth));
	context.writeLine(" px");
	return 0;
}

} // namespace lexxos::apps::device_programs
