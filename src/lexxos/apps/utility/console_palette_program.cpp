#include <lexxos/apps/utility/console_palette_program.h>

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

namespace {

struct ColorSample {
	int code{0};
	std::string_view name;
};

constexpr std::array<ColorSample, 8> normalForeground{
    {
     {30, "black"},
     {31, "red"},
     {32, "green"},
     {33, "yellow"},
     {34, "blue"},
     {35, "magenta"},
     {36, "cyan"},
     {37, "white"},
     }
};

constexpr std::array<ColorSample, 8> brightForeground{
    {
     {90, "br-black"},
     {91, "br-red"},
     {92, "br-green"},
     {93, "br-yellow"},
     {94, "br-blue"},
     {95, "br-magenta"},
     {96, "br-cyan"},
     {97, "br-white"},
     }
};

void writeSgr(lexxos::os::AppContext& context, int code) {
	context.write("\x1B[");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(code));
	context.write("m");
}

void writeSamples(lexxos::os::AppContext& context, std::string_view label, std::array<ColorSample, 8> const& colors) {
	context.write(label);
	context.writeLine(":");
	for (auto const& color : colors) {
		writeSgr(context, color.code);
		context.write(" ");
		context.write(color.name);
		context.write(" ");
		context.write("\x1B[0m");
	}
	context.writeLine("");
}

void writeBackgroundSamples(lexxos::os::AppContext& context, std::string_view label, std::array<ColorSample, 8> const& colors) {
	context.write(label);
	context.writeLine(":");
	for (auto const& color : colors) {
		writeSgr(context, color.code + 10);
		context.write(" ");
		context.write(color.name);
		context.write(" ");
		context.write("\x1B[0m");
	}
	context.writeLine("");
}

} // namespace

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor ConsolePaletteProgram::descriptor() {
	return {
	    .name = "console-palette",
	    .help = "show ANSI console palette",
	    .description = "Prints ANSI SGR foreground and background color samples for serial terminals and LCD console mirror.",
	    .usage = "console-palette",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int ConsolePaletteProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	context.writeLine("ANSI console palette");
	writeSamples(context, "foreground", normalForeground);
	writeSamples(context, "bright foreground", brightForeground);
	writeBackgroundSamples(context, "background", normalForeground);
	writeBackgroundSamples(context, "bright background", brightForeground);
	context.write("\x1B[0m");
	return 0;
}

} // namespace lexxos::apps::utility_programs
