#include <lexxos/program/program_manager.h>

#include <cstdint>

#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/shell/shell.h>

namespace lexxos::program {

namespace {

	bool startsWith(std::string_view value, std::string_view prefix) {
		return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
	}

	ProgramArgumentSpec const* findLongArgument(ProgramArgumentList const& arguments, std::string_view name) {
		for (std::size_t index = 0; index < arguments.size; ++index) {
			if (arguments.data[index].kind != ProgramArgumentKind::Positional && arguments.data[index].name == name) {
				return &arguments.data[index];
			}
		}
		return nullptr;
	}

	ProgramArgumentSpec const* findShortArgument(ProgramArgumentList const& arguments, char name) {
		for (std::size_t index = 0; index < arguments.size; ++index) {
			auto const& argument = arguments.data[index];
			if (argument.kind != ProgramArgumentKind::Positional && argument.shortName == name && name != 0) {
				return &argument;
			}
		}
		return nullptr;
	}

	ProgramArgumentSpec const* positionalArgument(ProgramArgumentList const& arguments, std::size_t requestedIndex) {
		std::size_t currentIndex = 0;
		for (std::size_t index = 0; index < arguments.size; ++index) {
			auto const& argument = arguments.data[index];
			if (argument.kind != ProgramArgumentKind::Positional) {
				continue;
			}
			if (currentIndex == requestedIndex) {
				return &argument;
			}
			++currentIndex;
		}
		return nullptr;
	}

	void writeUnsignedDecimal(lexxos::os::AppContext& context, std::uint32_t value) {
		char buffer[10]{};
		std::size_t index = sizeof(buffer);
		do {
			buffer[--index] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && index > 0);
		context.write({buffer + index, sizeof(buffer) - index});
	}

	void writeRuntimeProfile(lexxos::os::AppContext& context, ProgramDescriptor const& descriptor) {
		auto const& runtime = descriptor.runtime;
		if (runtime.launchMode == ProgramLaunchMode::Inline && !runtime.usesManagedMemory() && runtime.interfaceKind == ProgramInterfaceKind::Console && runtime.coreAffinity == anyProgramCore) {
			return;
		}

		context.writeLine("");
		context.writeLine("Runtime:");
		context.write("  launch: ");
		context.writeLine(toString(runtime.launchMode));
		context.write("  stack: ");
		writeUnsignedDecimal(context, runtime.stackSizeBytes);
		context.writeLine(" bytes");
		context.write("  priority: ");
		writeUnsignedDecimal(context, runtime.priority);
		context.writeLine("");
		context.write("  memory: ");
		context.writeLine(toString(runtime.memoryClass));
		context.write("  interface: ");
		context.writeLine(toString(runtime.interfaceKind));
		if (runtime.coreAffinity != anyProgramCore) {
			context.write("  core: ");
			writeUnsignedDecimal(context, static_cast<std::uint32_t>(runtime.coreAffinity));
			context.writeLine("");
		}
		if (runtime.externalMemoryQuotaBytes != 0) {
			context.write("  external quota: ");
			writeUnsignedDecimal(context, runtime.externalMemoryQuotaBytes);
			context.writeLine(" bytes");
		}
		if (runtime.internalMemoryQuotaBytes != 0) {
			context.write("  internal workspace: ");
			writeUnsignedDecimal(context, runtime.internalMemoryQuotaBytes);
			context.writeLine(" bytes");
		}
	}

	bool hasOptionName(std::string_view token) {
		return startsWith(token, "-") && token.size() > 1;
	}

	void appendDefaultValues(ParsedArguments& parsed, ProgramArgumentList const& arguments) {
		for (std::size_t index = 0; index < arguments.size; ++index) {
			auto const& argument = arguments.data[index];
			if (argument.kind != ProgramArgumentKind::Positional && !argument.defaultValue.empty() && !parsed.has(argument.name)) {
				parsed.setNamed(argument.name, argument.defaultValue, false);
			}
		}
	}

	ArgumentParseError parseArguments(ProgramDescriptor const& descriptor, ProgramInvocation const& invocation, ParsedArguments& parsed) {
		std::size_t positionalIndex = 0;
		bool parseOptions = true;

		auto appendPositional = [&](std::string_view token) -> ArgumentParseError {
			auto const* positional = positionalArgument(descriptor.arguments, positionalIndex);
			if (positional == nullptr && !descriptor.allowExtraArguments) {
				return {.status = ArgumentParseStatus::UnexpectedArgument, .token = token};
			}

			std::string_view const name = positional != nullptr ? positional->name : std::string_view{};
			if (!parsed.addPositional(name, token)) {
				return {.status = ArgumentParseStatus::TooManyArguments, .token = token};
			}
			++positionalIndex;
			return {};
		};

		appendDefaultValues(parsed, descriptor.arguments);

		for (int index = 1; index < invocation.argc; ++index) {
			std::string_view const token{invocation.argv[index]};
			if (parseOptions && (token == "--help" || token == "-h") && (!descriptor.allowExtraArguments || index == 1)) {
				return {.status = ArgumentParseStatus::HelpRequested, .token = token};
			}
			if (parseOptions && token == "--") {
				parseOptions = false;
				continue;
			}

			if (parseOptions && startsWith(token, "--") && token.size() > 2) {
				auto name = token.substr(2);
				std::string_view value{};
				auto const equalPosition = name.find('=');
				if (equalPosition != std::string_view::npos) {
					value = name.substr(equalPosition + 1);
					name = name.substr(0, equalPosition);
				}

				auto const* argument = findLongArgument(descriptor.arguments, name);
				if (argument == nullptr) {
					if (descriptor.allowExtraArguments) {
						auto const error = appendPositional(token);
						if (error.status != ArgumentParseStatus::Ok) {
							return error;
						}
						continue;
					}
					return {.status = ArgumentParseStatus::UnknownOption, .token = token, .argument = name};
				}

				if (argument->kind == ProgramArgumentKind::Flag) {
					if (equalPosition != std::string_view::npos) {
						return {.status = ArgumentParseStatus::UnexpectedArgument, .token = token, .argument = name};
					}
					if (!parsed.setNamed(argument->name, "true", true)) {
						return {.status = ArgumentParseStatus::TooManyArguments, .token = token, .argument = name};
					}
					continue;
				}

				if (equalPosition == std::string_view::npos) {
					if (index + 1 >= invocation.argc) {
						return {.status = ArgumentParseStatus::MissingValue, .token = token, .argument = name};
					}
					value = invocation.argv[++index];
					if (hasOptionName(value)) {
						return {.status = ArgumentParseStatus::MissingValue, .token = token, .argument = name};
					}
				}

				if (!parsed.setNamed(argument->name, value, true)) {
					return {.status = ArgumentParseStatus::TooManyArguments, .token = token, .argument = name};
				}
				continue;
			}

			if (parseOptions && startsWith(token, "-") && token.size() == 2) {
				auto const* argument = findShortArgument(descriptor.arguments, token[1]);
				if (argument == nullptr) {
					if (descriptor.allowExtraArguments) {
						auto const error = appendPositional(token);
						if (error.status != ArgumentParseStatus::Ok) {
							return error;
						}
						continue;
					}
					return {.status = ArgumentParseStatus::UnknownOption, .token = token, .argument = token.substr(1)};
				}

				if (argument->kind == ProgramArgumentKind::Flag) {
					if (!parsed.setNamed(argument->name, "true", true)) {
						return {.status = ArgumentParseStatus::TooManyArguments, .token = token, .argument = token.substr(1)};
					}
					continue;
				}

				if (index + 1 >= invocation.argc) {
					return {.status = ArgumentParseStatus::MissingValue, .token = token, .argument = token.substr(1)};
				}

				std::string_view const value{invocation.argv[++index]};
				if (hasOptionName(value)) {
					return {.status = ArgumentParseStatus::MissingValue, .token = token, .argument = token.substr(1)};
				}

				if (!parsed.setNamed(argument->name, value, true)) {
					return {.status = ArgumentParseStatus::TooManyArguments, .token = token, .argument = token.substr(1)};
				}
				continue;
			}

			auto const error = appendPositional(token);
			if (error.status != ArgumentParseStatus::Ok) {
				return error;
			}
		}

		for (std::size_t index = 0; index < descriptor.arguments.size; ++index) {
			auto const& argument = descriptor.arguments.data[index];
			if (!argument.required) {
				continue;
			}
			if (argument.kind == ProgramArgumentKind::Positional) {
				if (parsed.positional(argument.name).empty()) {
					return {.status = ArgumentParseStatus::MissingRequiredArgument, .argument = argument.name};
				}
			} else if (!parsed.has(argument.name) && argument.defaultValue.empty()) {
				return {.status = ArgumentParseStatus::MissingRequiredArgument, .argument = argument.name};
			}
		}

		return {};
	}

	void writeArgumentName(lexxos::os::AppContext& context, ProgramArgumentSpec const& argument) {
		if (argument.kind == ProgramArgumentKind::Positional) {
			context.write(argument.name);
			return;
		}

		if (argument.shortName != 0) {
			context.write("-");
			char value[] = {argument.shortName, '\0'};
			context.write(value);
			context.write(", ");
		}
		context.write("--");
		context.write(argument.name);
		if (argument.kind == ProgramArgumentKind::Option) {
			context.write(" <");
			context.write(argument.valueName.empty() ? "value" : argument.valueName);
			context.write(">");
		}
	}

	bool shouldLaunchThroughSupervisor(lexxos::os::AppContext const& context, ProgramDescriptor const& descriptor) {
		return context.owner() != lexxos::os::AppContextOwner::Program
		    && descriptor.runtime.launchMode != ProgramLaunchMode::Inline;
	}

	ProgramLaunchStatus launchStatusFromSupervisor(ProgramSupervisorStatus status) {
		switch (status) {
			case ProgramSupervisorStatus::Started:
				return ProgramLaunchStatus::Available;
			case ProgramSupervisorStatus::NotFound:
				return ProgramLaunchStatus::NotFound;
			case ProgramSupervisorStatus::Disabled:
				return ProgramLaunchStatus::Disabled;
			default:
				return ProgramLaunchStatus::InvalidRequest;
		}
	}

	ProgramExitResult executeThroughSupervisor(lexxos::os::AppContext& context, ProgramStartRequest const& request) {
		auto const result = supervisor().startForeground(context, request);
		if (!result.started()) {
			context.writeError(request.name);
			context.writeError(": start failed: ");
			context.writeErrorLine(toString(result.status));
			return {
			    .launchStatus = launchStatusFromSupervisor(result.status),
			    .exitCode = result.status == ProgramSupervisorStatus::NotFound ? 127 : 126,
			};
		}
		return result.exitResult;
	}

} // namespace

bool ProgramManager::registerProgram(ProgramDescriptor descriptor) {
	if (descriptor.name.empty() || descriptor.handler == nullptr || _size >= _programs.size()) {
		return false;
	}

	if (find(descriptor.name) != nullptr) {
		return false;
	}

	_programs[_size] = Entry{
	    .descriptor = descriptor,
	    .legacyCommand = {},
	    .kind = ProgramKind::Native,
	};
	++_size;
	return true;
}

bool ProgramManager::registerLegacyCommand(lexxos::shell::Command command) {
	if (command.name.empty() || command.handler == nullptr || _size >= _programs.size()) {
		return false;
	}

	if (find(command.name) != nullptr) {
		return false;
	}

	_programs[_size] = Entry{
	    .descriptor = ProgramDescriptor{
	                                    .name = command.name,
	                                    .help = command.help,
	                                    .description = command.help,
	                                    .usage = {},
	                                    .arguments = {},
	                                    .handler = nullptr,
	                                    .allowExtraArguments = true,
	                                    .legacy = true,
	                                    .enabled = true,
	                                    },
	    .legacyCommand = command,
	    .kind = ProgramKind::LegacyCommand,
	};
	++_size;
	return true;
}

ProgramDescriptor const* ProgramManager::find(std::string_view name) const {
	auto const* entry = _findEntry(name);
	return entry != nullptr ? &entry->descriptor : nullptr;
}

ProgramLaunchStatus ProgramManager::availability(std::string_view name) const {
	auto const* descriptor = find(name);
	if (descriptor == nullptr) {
		return ProgramLaunchStatus::NotFound;
	}
	if (!descriptor->enabled) {
		return ProgramLaunchStatus::Disabled;
	}
	return ProgramLaunchStatus::Available;
}

ProgramExitResult ProgramManager::execute(lexxos::os::AppContext& context, ProgramStartRequest const& request) {
	if (request.name.empty()) {
		return {.launchStatus = ProgramLaunchStatus::InvalidRequest, .exitCode = 1};
	}

	auto const* entry = _findEntry(request.name);
	if (entry == nullptr) {
		return {.launchStatus = ProgramLaunchStatus::NotFound, .exitCode = 127};
	}

	if (!entry->descriptor.enabled) {
		return {.launchStatus = ProgramLaunchStatus::Disabled, .exitCode = 126};
	}

	if (entry->kind == ProgramKind::Native && shouldLaunchThroughSupervisor(context, entry->descriptor)) {
		return executeThroughSupervisor(context, request);
	}

	auto executionContext = context.createChild({
	    .owner = context.owner(),
	    .shell = context.shell(),
	    .workingDirectory = context.workingDirectory(),
	    .standardInput = request.standardInput,
	    .standardOutput = request.standardOutput,
	    .standardError = request.standardError,
	    .cancellationToken = request.cancellationToken != nullptr ? request.cancellationToken : context.cancellationToken(),
	});

	ProgramInvocation invocation{
	    .argc = request.argc,
	    .argv = request.argv,
	};

	if (entry->kind == ProgramKind::LegacyCommand) {
		auto* shell = executionContext.shell();
		if (shell == nullptr) {
			return {.launchStatus = ProgramLaunchStatus::MissingContext, .exitCode = 126};
		}

		executionContext.write("legacy: ");
		executionContext.write(entry->descriptor.name);
		executionContext.writeLine(" uses deprecated command adapter");

		lexxos::shell::ShellContext shellContext{.shell = shell};
		lexxos::shell::CommandInvocation commandInvocation{
		    .argc = invocation.argc,
		    .argv = invocation.argv,
		};
		return {
		    .launchStatus = ProgramLaunchStatus::Available,
		    .exitCode = entry->legacyCommand.handler(shellContext, commandInvocation),
		};
	}

	ParsedArguments parsed;
	auto const parseError = parseArguments(entry->descriptor, invocation, parsed);
	if (parseError.status == ArgumentParseStatus::HelpRequested) {
		writeProgramHelp(executionContext, entry->descriptor);
		return {.launchStatus = ProgramLaunchStatus::Available, .exitCode = 0};
	}

	if (parseError.status != ArgumentParseStatus::Ok) {
		executionContext.writeError(entry->descriptor.name);
		executionContext.writeError(": ");
		executionContext.writeError(toString(parseError.status));
		if (!parseError.argument.empty()) {
			executionContext.writeError(": ");
			executionContext.writeError(parseError.argument);
		} else if (!parseError.token.empty()) {
			executionContext.writeError(": ");
			executionContext.writeError(parseError.token);
		}
		executionContext.writeErrorLine("");
		if (!entry->descriptor.usage.empty()) {
			executionContext.writeError("usage: ");
			executionContext.writeErrorLine(entry->descriptor.usage);
		}
		return {.launchStatus = ProgramLaunchStatus::InvalidArguments, .exitCode = 2};
	}

	return {
	    .launchStatus = ProgramLaunchStatus::Available,
	    .exitCode = entry->descriptor.handler(executionContext, invocation, parsed),
	};
}

void ProgramManager::writeProgramList(lexxos::os::AppContext& context) const {
	for (std::size_t index = 0; index < _size; ++index) {
		auto const& program = _programs[index].descriptor;
		context.write(program.name);
		if (program.legacy) {
			context.write(" (legacy)");
		}

		if (!program.enabled) {
			context.write(" (disabled)");
		}

		context.write(" - ");
		context.writeLine(program.help);
	}
}

void ProgramManager::writeProgramHelp(lexxos::os::AppContext& context, ProgramDescriptor const& descriptor) const {
	context.write(descriptor.name);
	if (descriptor.legacy) {
		context.write(" (legacy)");
	}

	if (!descriptor.enabled) {
		context.write(" (disabled)");
	}

	context.write(" - ");
	context.writeLine(descriptor.help);

	if (!descriptor.description.empty() && descriptor.description != descriptor.help) {
		context.writeLine("");
		context.writeLine(descriptor.description);
	}

	if (!descriptor.usage.empty()) {
		context.writeLine("");
		context.writeLine("Usage:");
		context.write("  ");
		context.writeLine(descriptor.usage);
	}

	if (descriptor.arguments.size != 0) {
		context.writeLine("");
		context.writeLine("Arguments:");
		for (std::size_t index = 0; index < descriptor.arguments.size; ++index) {
			auto const& argument = descriptor.arguments.data[index];
			context.write("  ");
			writeArgumentName(context, argument);
			if (argument.required) {
				context.write(" required");
			}

			if (!argument.defaultValue.empty()) {
				context.write(" default=");
				context.write(argument.defaultValue);
			}

			if (!argument.help.empty()) {
				context.write(" - ");
				context.write(argument.help);
			}

			context.writeLine("");
		}
	}

	writeRuntimeProfile(context, descriptor);
}

bool ProgramManager::visitPrograms(ProgramVisitor visitor, void* userData) const {
	if (visitor == nullptr) {
		return false;
	}

	for (std::size_t index = 0; index < _size; ++index) {
		if (!visitor(_programs[index].descriptor, userData)) {
			return false;
		}
	}

	return true;
}

std::size_t ProgramManager::size() const {
	return _size;
}

ProgramDescriptor const& ProgramManager::at(std::size_t index) const {
	return _programs[index].descriptor;
}

ProgramManager::Entry const* ProgramManager::_findEntry(std::string_view name) const {
	for (std::size_t index = 0; index < _size; ++index) {
		if (_programs[index].descriptor.name == name) {
			return &_programs[index];
		}
	}

	return nullptr;
}

ProgramManager::Entry* ProgramManager::_findEntry(std::string_view name) {
	for (std::size_t index = 0; index < _size; ++index) {
		if (_programs[index].descriptor.name == name) {
			return &_programs[index];
		}
	}

	return nullptr;
}

} // namespace lexxos::program
