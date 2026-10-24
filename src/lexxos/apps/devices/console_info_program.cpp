#include <lexxos/apps/devices/console_info_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/app/picocalc_lcd_console.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	void writeBool(lexxos::os::AppContext& context, bool value) {
		context.writeLine(value ? "yes" : "no");
	}

	void writeSize(lexxos::os::AppContext& context, std::size_t value) {
		writeUnsigned(context, static_cast<std::uint32_t>(value));
	}

	void writeLabel(lexxos::os::AppContext& context, std::string_view label) {
		context.write(label);
		context.write(": ");
	}

} // namespace

lexxos::program::ProgramDescriptor ConsoleInfoProgram::descriptor() {
	return {
	    .name = "console-info",
	    .help = "show LCD console state",
	    .description = "Shows the current LCD console geometry, cursor, status bar, scrollback and hardware scroll state.",
	    .usage = "console-info",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int ConsoleInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const info = lexxos::app::picocalc_lcd_console::info();

	writeLabel(context, "ready");
	writeBool(context, info.ready);

	writeLabel(context, "text mode");
	writeBool(context, info.textMode);

	writeLabel(context, "size");
	writeUnsigned(context, static_cast<std::uint32_t>(info.columns));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(info.rows));
	context.writeLine("");

	writeLabel(context, "logical columns");
	writeUnsigned(context, static_cast<std::uint32_t>(info.logicalColumns));
	context.writeLine("");

	writeLabel(context, "view left");
	writeUnsigned(context, static_cast<std::uint32_t>(info.viewLeftColumn));
	context.write("/");
	writeUnsigned(context, static_cast<std::uint32_t>(info.maxViewLeftColumn));
	context.writeLine("");

	writeLabel(context, "cursor");
	writeUnsigned(context, static_cast<std::uint32_t>(info.cursorX));
	context.write(",");
	writeUnsigned(context, static_cast<std::uint32_t>(info.cursorY));
	context.writeLine("");

	writeLabel(context, "queue");
	writeSize(context, info.queueCount);
	context.writeLine("");

	writeLabel(context, "scrollback");
	writeSize(context, info.scrollbackCount);
	context.write("/");
	writeSize(context, info.scrollbackCapacity);
	context.writeLine("");

	writeLabel(context, "scrollback dropped");
	writeUnsigned(context, info.scrollbackDroppedRows);
	context.writeLine("");

	writeLabel(context, "view");
	context.writeLine(info.viewFollowTail ? "live" : "history");

	writeLabel(context, "view top");
	writeUnsigned(context, static_cast<std::uint32_t>(info.viewTopLine));
	context.writeLine("");

	writeLabel(context, "cursor draw");
	writeBool(context, info.drawCursor);

	writeLabel(context, "cursor visible");
	writeBool(context, info.cursorVisible);

	writeLabel(context, "hardware scroll");
	writeBool(context, info.hardwareScrollEnabled);

	writeLabel(context, "scroll area");
	writeUnsigned(context, static_cast<std::uint32_t>(info.scrollAreaPixels));
	context.writeLine("");

	writeLabel(context, "scroll start");
	writeUnsigned(context, static_cast<std::uint32_t>(info.scrollStartPixels));
	context.writeLine("");

	writeLabel(context, "status bar");
	writeBool(context, info.statusBarVisible);

	writeLabel(context, "status bar pixels");
	writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarPixels));
	context.writeLine("");

	writeLabel(context, "status updated");
	writeUnsigned(context, info.statusBarUpdatedMs);
	context.writeLine(" ms");

	writeLabel(context, "status battery");
	if (!info.statusBarBatteryValid) {
		context.writeLine("unavailable");
	} else {
		writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarBatteryPercent));
		context.write("% charging=");
		context.writeLine(info.statusBarBatteryCharging ? "yes" : "no");
	}

	writeLabel(context, "status wifi");
	if (!info.statusBarWifiValid) {
		context.writeLine("unavailable");
	} else {
		context.write(info.statusBarWifiConnected ? "connected level=" : "disconnected level=");
		writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarWifiSignalLevel));
		context.write("/3");
		context.writeLine("");
	}

	writeLabel(context, "status time");
	if (!info.statusBarTimeValid) {
		context.writeLine("unavailable");
	} else {
		if (info.statusBarHour < 10) {
			context.write("0");
		}
		writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarHour));
		context.write(":");
		if (info.statusBarMinute < 10) {
			context.write("0");
		}
		writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarMinute));
		context.write(":");
		if (info.statusBarSecond < 10) {
			context.write("0");
		}
		writeUnsigned(context, static_cast<std::uint32_t>(info.statusBarSecond));
		context.writeLine("");
	}

	return 0;
}

} // namespace lexxos::apps::device_programs
