#include <lexxos/apps/lexx/lexx_run_program.h>

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::lexx_programs {
namespace {

	bool equals(std::string_view value, std::string_view expected) {
		return value == expected;
	}

	bool startsWith(std::string_view value, std::string_view prefix) {
		return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
	}

	bool isOptionLike(std::string_view value) {
		return startsWith(value, "-") && value != "-";
	}

	bool endsWith(std::string_view value, std::string_view suffix) {
		return value.size() >= suffix.size()
		    && value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
	}

	bool isLexxExecutablePath(std::string_view value) {
		return endsWith(value, ".lexx");
	}

	bool parseMaxSteps(std::string_view text, std::uint32_t& output) {
		return lexxos::apps::parseUnsigned32(text, output);
	}

	void writeUsage(lexxos::os::AppContext& context) {
		context.writeErrorLine("usage: lexx-run [--trace] [--max-steps <count>] <file.lexx> [args...]");
	}

	void writeUnsignedError(lexxos::os::AppContext& context, std::uint32_t value) {
		char buffer[10]{};
		std::size_t index = sizeof(buffer);
		do {
			buffer[--index] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && index > 0);
		context.writeError({buffer + index, sizeof(buffer) - index});
	}

	void writeHexNibbleError(lexxos::os::AppContext& context, std::uint8_t value) {
		char const text[] = {static_cast<char>(value < 10 ? ('0' + value) : ('a' + value - 10)), '\0'};
		context.writeError(text);
	}

	void writeHex32Error(lexxos::os::AppContext& context, std::uint32_t value) {
		context.writeError("0x");
		for (int shift = 28; shift >= 0; shift -= 4) {
			writeHexNibbleError(context, static_cast<std::uint8_t>((value >> static_cast<unsigned>(shift)) & 0x0FU));
		}
	}

	void writeFailureDetails(lexxos::os::AppContext& context, lexxos::external_program::ExternalProgramResult const& result) {
		if (result.faultAddress != 0 || result.faultInstruction != 0) {
			context.writeError(" fault-address=");
			writeHex32Error(context, result.faultAddress);
			if (result.faultInstruction != 0) {
				context.writeError(" instruction=");
				writeHex32Error(context, result.faultInstruction);
			}
		}
		if (result.unsupportedSyscall != 0) {
			context.writeError(" syscall=");
			writeUnsignedError(context, result.unsupportedSyscall);
		}
		context.writeError(" executed=");
		writeUnsignedError(context, result.instructionsExecuted);
		context.writeErrorLine("");
	}

} // namespace

lexxos::program::ProgramDescriptor LexxRunProgram::descriptor() {
	return {
	    .name = "lexx-run",
	    .help = "run a .lexx RV32I program",
	    .description = "Loads a LexxOS external executable from VFS, validates its LEXX header, allocates a bounded guest memory arena and executes RV32I code through the text-mode ABI.",
	    .usage = "lexx-run [--trace] [--max-steps <count>] <file.lexx> [args...]",
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::ExternalPreferred,
	                .externalMemoryQuotaBytes = lexxos::external_program::lexxMaximumGuestMemoryBytes,
	                .internalMemoryQuotaBytes = lexxos::external_program::lexxMaximumGuestMemoryBytes,
	                .cancellable = true,
	                .allowBackground = true,
	                .interfaceKind = lexxos::program::ProgramInterfaceKind::Console,
	                },
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int LexxRunProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argc < 1 || invocation.argv == nullptr) {
		writeUsage(context);
		return 2;
	}

	lexxos::external_program::ExternalProgramOptions options{};
	int pathIndex = 1;
	auto const autoInvocation = isLexxExecutablePath(invocation.argv[0]);
	if (autoInvocation) {
		pathIndex = 0;
	} else {
		if (invocation.argc < 2) {
			writeUsage(context);
			return 2;
		}
		while (pathIndex < invocation.argc) {
			std::string_view const token{invocation.argv[pathIndex]};
			if (equals(token, "--trace")) {
				options.trace = true;
				++pathIndex;
				continue;
			}
			if (equals(token, "--max-steps")) {
				if (pathIndex + 1 >= invocation.argc || !parseMaxSteps(invocation.argv[pathIndex + 1], options.maxInstructions)) {
					context.writeErrorLine("lexx-run: invalid --max-steps value");
					return 2;
				}
				pathIndex += 2;
				continue;
			}
			if (startsWith(token, "--max-steps=")) {
				if (!parseMaxSteps(token.substr(12), options.maxInstructions)) {
					context.writeErrorLine("lexx-run: invalid --max-steps value");
					return 2;
				}
				++pathIndex;
				continue;
			}
			if (equals(token, "--")) {
				++pathIndex;
				break;
			}
			if (isOptionLike(token)) {
				context.writeError("lexx-run: unknown option: ");
				context.writeErrorLine(token);
				writeUsage(context);
				return 2;
			}
			break;
		}
	}

	if (pathIndex >= invocation.argc) {
		writeUsage(context);
		return 2;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!lexxos::apps::resolvePath(context, invocation.argv[pathIndex], path)) {
		context.writeErrorLine("lexx-run: invalid path");
		return 2;
	}

	options.guestArgc = invocation.argc - pathIndex;
	options.guestArgv = invocation.argv + pathIndex;

	auto const result = lexxos::external_program::runLexxExecutable(context, path.data(), options);
	if (result.status == lexxos::external_program::ExternalProgramStatus::Ok) {
		return result.exitCode;
	}

	if (result.status == lexxos::external_program::ExternalProgramStatus::Cancelled) {
		context.writeError("lexx-run: cancelled after ");
		writeUnsignedError(context, result.instructionsExecuted);
		context.writeErrorLine(" instructions");
		return 130;
	}

	context.writeError("lexx-run: failed: ");
	context.writeError(lexxos::external_program::toString(result.status));
	writeFailureDetails(context, result);
	return 126;
}

} // namespace lexxos::apps::lexx_programs
