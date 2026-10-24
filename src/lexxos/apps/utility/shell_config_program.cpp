#include <lexxos/apps/utility/shell_config_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/shell/shell.h>

namespace {

bool equals(std::string_view left, std::string_view right) {
	return left == right;
}

void writeShellConfigStatus(lexxos::os::AppContext& context, lexxos::shell::Shell const& shell) {
	context.write("path: ");
	context.writeLine(lexxos::shell::Shell::configPath);
	context.write("loaded: ");
	context.writeLine(shell.shellConfigLoaded() ? "yes" : "no");
	context.write("autosuggest: ");
	context.writeLine(shell.autosuggestEnabled() ? "on" : "off");
	context.write("history: ");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(shell.historySize()));
	context.writeLine(" entries");
}

} // namespace

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor ShellConfigProgram::descriptor() {
	return {
	    .name = "shell-config",
	    .help = "show or update shell settings",
	    .description = "Shows or updates shell autosuggestion settings and saves or reloads shell history.",
	    .usage = "shell-config [show|load|save|autosuggest on|off]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int ShellConfigProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	auto* shell = context.shell();
	if (shell == nullptr) {
		context.writeLine("shell-config: missing shell context");
		return 1;
	}

	if (invocation.argc <= 1 || equals(invocation.argv[1], "show")) {
		writeShellConfigStatus(context, *shell);
		return 0;
	}

	std::string_view const command = invocation.argv[1];
	if (equals(command, "load")) {
		if (!shell->loadShellConfig(true)) {
			context.writeLine("shell-config: load failed");
			return 1;
		}
		writeShellConfigStatus(context, *shell);
		return 0;
	}

	if (equals(command, "save")) {
		if (!shell->saveShellConfig()) {
			context.writeLine("shell-config: save failed");
			return 1;
		}
		writeShellConfigStatus(context, *shell);
		return 0;
	}

	if (equals(command, "autosuggest")) {
		if (invocation.argc != 3) {
			context.writeLine("usage: shell-config autosuggest on|off");
			return 1;
		}
		std::string_view const value = invocation.argv[2];
		if (equals(value, "on")) {
			shell->setAutosuggestEnabled(true);
		} else if (equals(value, "off")) {
			shell->setAutosuggestEnabled(false);
		} else {
			context.writeLine("usage: shell-config autosuggest on|off");
			return 1;
		}
		writeShellConfigStatus(context, *shell);
		return 0;
	}

	context.writeLine("usage: shell-config [show|load|save|autosuggest on|off]");
	return 1;
}

} // namespace lexxos::apps::utility_programs
