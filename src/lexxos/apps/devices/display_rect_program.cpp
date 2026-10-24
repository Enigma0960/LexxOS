#include <lexxos/apps/devices/display_rect_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec displayRectArguments[] = {
	    {     .name = "x",      .valueName = "pixels",                        .help = "left coordinate", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,  .required = true},
	    {     .name = "y",      .valueName = "pixels",                         .help = "top coordinate", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,  .required = true},
	    { .name = "width",      .valueName = "pixels",                        .help = "rectangle width", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,  .required = true},
	    {.name = "height",      .valueName = "pixels",                       .help = "rectangle height", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,  .required = true},
	    { .name = "color", .valueName = "rgb565|name", .help = "fill color: rgb565 value or color name", .kind = lexxos::program::ProgramArgumentKind::Positional,   .valueType = lexxos::program::ProgramArgumentValueType::String,  .required = true},
	    {  .name = "name",        .valueName = "name",                    .help = "display device name", .kind = lexxos::program::ProgramArgumentKind::Positional,   .valueType = lexxos::program::ProgramArgumentValueType::String, .required = false},
	};

} // namespace

lexxos::program::ProgramDescriptor DisplayRectProgram::descriptor() {
	return {
	    .name = "display-rect",
	    .help = "draw a filled rectangle",
	    .description = "Draws a filled RGB565 rectangle without using a full framebuffer.",
	    .usage = "display-rect <x> <y> <width> <height> <color> [name]",
	    .arguments = argumentList(displayRectArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayRectProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = detail::selectedDisplay(context, arguments.positional("name"));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "display-rect: display device is not registered"
		        : "display-rect: display device is not ready");
		return 1;
	}

	// Parse and validate rectangle geometry and color.
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint16_t color = 0;
	if (!detail::parseUnsigned(arguments.positional("x"), x)
	    || !detail::parseUnsigned(arguments.positional("y"), y)
	    || !detail::parseUnsigned(arguments.positional("width"), width)
	    || !detail::parseUnsigned(arguments.positional("height"), height)
	    || !detail::parseRgb565Color(arguments.positional("color"), color)) {
		context.writeErrorLine("display-rect: invalid argument");
		return 1;
	}
	if (width == 0 || height == 0 || x > 0x7FFFFFFFU || y > 0x7FFFFFFFU || width > 0x7FFFFFFFU || height > 0x7FFFFFFFU) {
		context.writeErrorLine("display-rect: invalid rectangle");
		return 1;
	}

	lexxos::devices::Rect const rect{
	    .x = static_cast<int>(x),
	    .y = static_cast<int>(y),
	    .width = static_cast<int>(width),
	    .height = static_cast<int>(height),
	};

	auto diagnosticLocks = detail::lockDisplayDiagnosticResources(context, *display, "display-rect");
	if (!diagnosticLocks) {
		context.writeErrorLine("display-rect: display or input device is busy");
		return 1;
	}

	bool drawn = false;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		drawn = detail::drawFilledRect(*display, rect, color);
	}

	if (!drawn) {
		context.writeErrorLine("display-rect: draw failed");
		return 1;
	}
	context.writeLine("display-rect: ok");
	return 0;
}

} // namespace lexxos::apps::device_programs
