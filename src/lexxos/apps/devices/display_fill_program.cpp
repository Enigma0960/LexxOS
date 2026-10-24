#include <lexxos/apps/devices/display_fill_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec displayFillArguments[] = {
	    {.name = "color", .valueName = "rgb565|name", .help = "fill color: rgb565 value or color name", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::String,  .required = true},
	    { .name = "name",        .valueName = "name",                    .help = "display device name", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::String, .required = false},
	};

} // namespace

lexxos::program::ProgramDescriptor DisplayFillProgram::descriptor() {
	return {
	    .name = "display-fill",
	    .help = "fill display with one color",
	    .description = "Clears the selected display using a named color or raw RGB565 value.",
	    .usage = "display-fill <color> [name]",
	    .arguments = argumentList(displayFillArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayFillProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = detail::selectedDisplay(context, arguments.positional("name"));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "display-fill: display device is not registered"
		        : "display-fill: display device is not ready");
		return 1;
	}

	std::uint16_t color = 0;
	if (!detail::parseRgb565Color(arguments.positional("color"), color)) {
		context.writeErrorLine("display-fill: invalid color");
		return 1;
	}

	auto diagnosticLocks = detail::lockDisplayDiagnosticResources(context, *display, "display-fill");
	if (!diagnosticLocks) {
		context.writeErrorLine("display-fill: display or input device is busy");
		return 1;
	}

	bool cleared = false;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		cleared = display->clear(color);
	}

	if (!cleared) {
		context.writeErrorLine("display-fill: failed to clear display");
		return 1;
	}
	context.writeLine("display-fill: ok");
	return 0;
}

} // namespace lexxos::apps::device_programs
