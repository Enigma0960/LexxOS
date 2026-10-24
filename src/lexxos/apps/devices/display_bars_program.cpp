#include <lexxos/apps/devices/display_bars_program.h>

#include <cstdint>
#include <iterator>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec optionalNameArgument[] = {
	    {.name = "name", .valueName = "name", .help = "device name", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::String, .required = false},
	};

} // namespace

lexxos::program::ProgramDescriptor DisplayBarsProgram::descriptor() {
	return {
	    .name = "display-bars",
	    .help = "draw display color bars",
	    .description = "Draws a simple RGB565 color bar pattern for display diagnostics.",
	    .usage = "display-bars [name]",
	    .arguments = argumentList(optionalNameArgument),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayBarsProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = detail::selectedDisplay(context, arguments.positional("name"));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "display-bars: display device is not registered"
		        : "display-bars: display device is not ready");
		return 1;
	}

	auto* line = detail::displayScratchLine();
	auto const width = display->width();
	auto const height = display->height();
	if (width <= 0 || height <= 0 || width > detail::displayScratchLinePixels) {
		context.writeErrorLine("display-bars: unsupported display size");
		return 1;
	}

	constexpr std::uint16_t colors[] = {0xF800, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F, 0xFFFF, 0x0000};
	constexpr int barCount = static_cast<int>(std::size(colors));
	for (int x = 0; x < width; ++x) {
		line[x] = colors[(x * barCount) / width];
	}

	auto diagnosticLocks = detail::lockDisplayDiagnosticResources(context, *display, "display-bars");
	if (!diagnosticLocks) {
		context.writeErrorLine("display-bars: display or input device is busy");
		return 1;
	}

	char const* error = nullptr;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;

		if (!display->beginFrame()) {
			error = "display-bars: failed to begin frame";
		} else {
			for (int y = 0; y < height; ++y) {
				if (!display->blitRgb565({.x = 0, .y = y, .width = width, .height = 1}, line, width)) {
					error = "display-bars: draw failed";
					break;
				}
			}
			if (error == nullptr && !display->endFrame()) {
				error = "display-bars: failed to end frame";
			}
		}
	}

	if (error != nullptr) {
		context.writeErrorLine(error);
		return 1;
	}
	context.writeLine("display-bars: ok");
	return 0;
}

} // namespace lexxos::apps::device_programs
