#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/arguments.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::io {
class InputStream;
class OutputStream;
} // namespace lexxos::io

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

class CancellationToken;

struct ProgramInvocation {
	int argc{0};
	char const* const* argv{nullptr};
};

enum class ProgramLaunchStatus : std::uint8_t {
	Available,
	NotFound,
	Disabled,
	InvalidRequest,
	MissingContext,
	InvalidArguments,
};

enum class ProgramLaunchMode : std::uint8_t {
	Inline,
	ForegroundTask,
	BackgroundTask,
};

enum class ProgramMemoryClass : std::uint8_t {
	InternalOnly,
	ExternalPreferred,
	ExternalRequired,
};

enum class ProgramInterfaceKind : std::uint8_t {
	Console,
	Graphical,
};

inline constexpr int anyProgramCore = -1;

struct ProgramRuntimeProfile {
	ProgramLaunchMode launchMode{ProgramLaunchMode::Inline};
	std::uint32_t stackSizeBytes{
	    lexxos::runtime::standardRuntimeProfile.programs.defaultStackSizeBytes};
	std::uint32_t priority{1};
	ProgramMemoryClass memoryClass{ProgramMemoryClass::InternalOnly};
	std::uint32_t externalMemoryQuotaBytes{
	    lexxos::runtime::standardRuntimeProfile.programs.defaultExternalWorkspaceBytes};
	std::uint32_t internalMemoryQuotaBytes{
	    lexxos::runtime::standardRuntimeProfile.programs.defaultInternalWorkspaceBytes};
	bool cancellable{true};
	bool allowBackground{false};
	ProgramInterfaceKind interfaceKind{ProgramInterfaceKind::Console};
	int coreAffinity{anyProgramCore};

	[[nodiscard]] bool usesExternalMemory() const {
		return memoryClass != ProgramMemoryClass::InternalOnly || externalMemoryQuotaBytes != 0;
	}

	[[nodiscard]] bool usesManagedMemory() const {
		return usesExternalMemory() || internalMemoryQuotaBytes != 0;
	}
};

std::string_view toString(ProgramLaunchStatus status);
std::string_view toString(ProgramLaunchMode mode);
std::string_view toString(ProgramMemoryClass memoryClass);
std::string_view toString(ProgramInterfaceKind interfaceKind);

struct ProgramExitResult {
	ProgramLaunchStatus launchStatus{ProgramLaunchStatus::Available};
	int exitCode{0};

	[[nodiscard]] bool ok() const {
		return launchStatus == ProgramLaunchStatus::Available && exitCode == 0;
	}
};

struct ProgramStartRequest {
	std::string_view name;
	int argc{0};
	char const* const* argv{nullptr};
	lexxos::io::InputStream* standardInput{nullptr};
	lexxos::io::OutputStream* standardOutput{nullptr};
	lexxos::io::OutputStream* standardError{nullptr};
	CancellationToken* cancellationToken{nullptr};
};

using ProgramHandler = int (*)(lexxos::os::AppContext& context, ProgramInvocation const& invocation, ParsedArguments const& arguments);

struct ProgramDescriptor {
	std::string_view name;
	std::string_view help;
	std::string_view description{};
	std::string_view usage{};
	ProgramArgumentList arguments{};
	ProgramHandler handler{nullptr};
	ProgramRuntimeProfile runtime{};
	bool allowExtraArguments{false};
	bool legacy{false};
	bool enabled{true};
};

} // namespace lexxos::program
