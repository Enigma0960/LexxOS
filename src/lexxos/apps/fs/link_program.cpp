#include <lexxos/apps/fs/link_program.h>

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/shortcuts/shortcut.h>

namespace lexxos::apps::filesystem_programs {

namespace {

	using Path = std::array<char, lexxos::vfs::maxPathLength>;

	bool equals(char const* left, char const* right) {
		return std::strcmp(left, right) == 0;
	}

	bool resolve(lexxos::os::AppContext& ctx, char const* input, Path& output) {
		if (lexxos::apps::resolvePath(ctx, input, output)) {
			return true;
		}

		ctx.writeErrorLine("link: invalid path");
		return false;
	}

	bool copyArg(char const* input, std::array<char, lexxos::shortcuts::maxArgumentBytes>& output) {
		auto const length = std::strlen(input);
		if (length >= output.size()) {
			return false;
		}

		std::memcpy(output.data(), input, length + 1);
		return true;
	}

	int failure(lexxos::os::AppContext& ctx, lexxos::shortcuts::Status status) {
		ctx.writeError("link: ");
		ctx.writeErrorLine(lexxos::shortcuts::statusText(status));
		return 1;
	}

} // namespace

lexxos::program::ProgramDescriptor LinkProgram::descriptor() {
	return {
	    .name = "link",
	    .help = "create, inspect or launch .lnk shortcuts",
	    .usage = "link create <link.lnk> <target> [--bg|--fg] [--delay-ms N] [-- args...] | show <link.lnk> | run <link.lnk> [args...]",
	    .handler = run,
	    .runtime = {.launchMode = lexxos::program::ProgramLaunchMode::Inline},
	    .allowExtraArguments = true,
	};
}

int LinkProgram::run(lexxos::os::AppContext& ctx,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argc < 3) {
		ctx.writeErrorLine("usage: link create|show|run <path.lnk> ...");
		return 2;
	}

	char const* command = invocation.argv[1];
	Path path{};

	if (!resolve(ctx, invocation.argv[2], path)) {
		return 2;
	}

	if (equals(command, "show") || equals(command, "run")) {
		lexxos::shortcuts::Shortcut shortcut{};
		auto status = lexxos::shortcuts::load(path.data(), shortcut);
		if (status != lexxos::shortcuts::Status::Ok) {
			return failure(ctx, status);
		}

		if (equals(command, "run")) {
			auto launched = lexxos::shortcuts::launch(ctx, shortcut,
			    invocation.argc - 3, invocation.argv + 3);
			return launched == lexxos::shortcuts::Status::Ok ? 0 : 1;
		}

		ctx.write("target: ");
		ctx.writeLine(shortcut.target.data());
		ctx.write("mode: ");
		ctx.writeLine(shortcut.background ? "background" : "foreground");
		ctx.write("delay_ms: ");
		lexxos::apps::writeUnsigned(ctx, shortcut.delayMs);
		ctx.writeLine("");

		for (std::size_t i = 0; i < shortcut.argc; ++i) {
			ctx.write("arg: ");
			ctx.writeLine(shortcut.args[i].data());
		}

		return 0;
	}

	if (!equals(command, "create") || invocation.argc < 4) {
		ctx.writeErrorLine("usage: link create <path.lnk> <target> [--bg|--fg] [--delay-ms N] [-- args...]");
		return 2;
	}

	lexxos::shortcuts::Shortcut shortcut{};
	std::string_view const target = invocation.argv[3];

	// Absolute paths and executable paths with '/' are normalized at creation.
	if (target.find('/') != std::string_view::npos || target.ends_with(".lexx")) {
		Path normalized{};
		if (!resolve(ctx, invocation.argv[3], normalized)) {
			return 2;
		}

		std::memcpy(shortcut.target.data(), normalized.data(), std::strlen(normalized.data()) + 1);
	} else {
		if (target.size() >= shortcut.target.size()) {
			return 2;
		}

		std::memcpy(shortcut.target.data(), target.data(), target.size());
		shortcut.target[target.size()] = '\0';
	}

	bool argumentMode = false;
	for (int i = 4; i < invocation.argc; ++i) {
		auto const token = std::string_view(invocation.argv[i]);
		if (!argumentMode && token == "--") {
			argumentMode = true;
			continue;
		}

		if (!argumentMode && (token == "--bg" || token == "--background")) {
			shortcut.background = true;
			continue;
		}

		if (!argumentMode && (token == "--fg" || token == "--foreground")) {
			shortcut.background = false;
			continue;
		}

		if (!argumentMode && token == "--delay-ms" && i + 1 < invocation.argc) {
			if (!lexxos::shortcuts::parseDelay(invocation.argv[++i], shortcut.delayMs)) {
				ctx.writeErrorLine("link: invalid delay");
				return 2;
			}

			continue;
		}

		if (!argumentMode) {
			ctx.writeErrorLine("link: specify program arguments after --");
			return 2;
		}

		if (shortcut.argc >= shortcut.args.size() || !copyArg(invocation.argv[i], shortcut.args[shortcut.argc])) {
			ctx.writeErrorLine("link: too many or too long arguments");
			return 2;
		}

		++shortcut.argc;
	}

	auto saved = lexxos::shortcuts::save(path.data(), shortcut);
	return saved == lexxos::shortcuts::Status::Ok ? 0 : failure(ctx, saved);
}

} // namespace lexxos::apps::filesystem_programs
