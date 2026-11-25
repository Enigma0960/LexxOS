#include <lexxos/apps/core/autostart_program.h>

#include <cstring>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/shortcuts/shortcut.h>

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor AutostartProgram::descriptor() {
	return {
	    .name = "autostart",
	    .help = "configure startup shortcuts",
	    .usage = "autostart [status|delay <ms>|enable|disable]",
	    .handler = run,
	    .allowExtraArguments = true,
	};
}

int AutostartProgram::run(lexxos::os::AppContext& ctx,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	auto config = lexxos::shortcuts::loadConfig();
	if (invocation.argc == 1 || (invocation.argc == 2 && std::strcmp(invocation.argv[1], "status") == 0)) {
		ctx.write("autostart: ");
		ctx.writeLine(config.enabled ? "enabled" : "disabled");
		ctx.write("delay_ms: ");
		lexxos::apps::writeUnsigned(ctx, config.delayMs);
		ctx.writeLine("");
		ctx.writeLine("directory: /sys/autostart");
		ctx.writeLine("config: /mnt/sd/.lexx/autostart.conf");
		return 0;
	}

	if (invocation.argc == 3 && std::strcmp(invocation.argv[1], "delay") == 0) {
		if (!lexxos::shortcuts::parseDelay(invocation.argv[2], config.delayMs)) {
			ctx.writeErrorLine("autostart: invalid delay in milliseconds");
			return 2;
		}
	} else if (invocation.argc == 2 && std::strcmp(invocation.argv[1], "enable") == 0) {
		config.enabled = true;
	} else if (invocation.argc == 2 && std::strcmp(invocation.argv[1], "disable") == 0) {
		config.enabled = false;
	} else {
		ctx.writeErrorLine("usage: autostart [status|delay <ms>|enable|disable]");
		return 2;
	}

	if (!lexxos::shortcuts::saveConfig(config)) {
		ctx.writeErrorLine("autostart: unable to write config (is SD mounted read-write?)");
		return 1;
	}

	ctx.writeLine("autostart: configuration saved (effective at next boot)");
	return 0;
}

} // namespace lexxos::apps::system_programs
