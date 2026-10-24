#include <lexxos/program/program.h>

namespace lexxos::program {

std::string_view toString(ProgramLaunchStatus status) {
	switch (status) {
		case ProgramLaunchStatus::Available:
			return "available";
		case ProgramLaunchStatus::NotFound:
			return "not-found";
		case ProgramLaunchStatus::Disabled:
			return "disabled";
		case ProgramLaunchStatus::InvalidRequest:
			return "invalid-request";
		case ProgramLaunchStatus::MissingContext:
			return "missing-context";
		case ProgramLaunchStatus::InvalidArguments:
			return "invalid-arguments";
	}

	return "unknown";
}

std::string_view toString(ProgramLaunchMode mode) {
	switch (mode) {
		case ProgramLaunchMode::Inline:
			return "inline";
		case ProgramLaunchMode::ForegroundTask:
			return "foreground-task";
		case ProgramLaunchMode::BackgroundTask:
			return "background-task";
	}

	return "unknown";
}

std::string_view toString(ProgramMemoryClass memoryClass) {
	switch (memoryClass) {
		case ProgramMemoryClass::InternalOnly:
			return "internal-only";
		case ProgramMemoryClass::ExternalPreferred:
			return "external-preferred";
		case ProgramMemoryClass::ExternalRequired:
			return "external-required";
	}

	return "unknown";
}

std::string_view toString(ProgramInterfaceKind interfaceKind) {
	switch (interfaceKind) {
		case ProgramInterfaceKind::Console:
			return "console";
		case ProgramInterfaceKind::Graphical:
			return "graphical";
	}

	return "unknown";
}

} // namespace lexxos::program
