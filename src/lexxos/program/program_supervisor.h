#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/program.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::program {

using ProgramProcessId = std::uint32_t;

enum class ProgramProcessState : std::uint8_t {
	Empty,
	Starting,
	Running,
	StopRequested,
	Finished,
};

std::string_view toString(ProgramProcessState state);

enum class ProgramSupervisorStatus : std::uint8_t {
	Started,
	InvalidRequest,
	NotFound,
	Disabled,
	AlreadyRunning,
	BackgroundNotAllowed,
	TooManyProcesses,
	ArgumentsTooLarge,
	ExternalMemoryUnavailable,
	MemoryUnavailable,
	TaskCreateFailed,
};

std::string_view toString(ProgramSupervisorStatus status);

struct ProgramProcessSnapshot {
	ProgramProcessId id{0};
	ProgramProcessState state{ProgramProcessState::Empty};
	std::string_view name{};
	ProgramLaunchStatus launchStatus{ProgramLaunchStatus::Available};
	int exitCode{0};
	std::uint32_t stackSizeBytes{0};
	std::uint32_t stackFreeBytes{0};
	std::uint32_t priority{0};
	ProgramLaunchMode launchMode{ProgramLaunchMode::Inline};
	ProgramMemoryClass memoryClass{ProgramMemoryClass::InternalOnly};
	std::uint32_t externalMemoryQuotaBytes{0};
	std::uint32_t externalMemoryReservedBytes{0};
	std::uint32_t internalMemoryQuotaBytes{0};
	std::uint32_t internalMemoryReservedBytes{0};
	ProgramMemoryArenaStatus memoryArenaStatus{ProgramMemoryArenaStatus::Ready};
	ProgramInterfaceKind interfaceKind{ProgramInterfaceKind::Console};
};

enum class ProgramFocusTarget : std::uint8_t {
	Shell,
	Program,
};

struct ProgramFocusSnapshot {
	ProgramFocusTarget target{ProgramFocusTarget::Shell};
	ProgramProcessId processId{0};
	std::string_view processName{};
	std::size_t activeProgramCount{0};
	ProgramInterfaceKind interfaceKind{ProgramInterfaceKind::Console};

	[[nodiscard]] bool programFocused() const {
		return target == ProgramFocusTarget::Program && processId != 0;
	}

	[[nodiscard]] bool graphicalProgramFocused() const {
		return programFocused() && interfaceKind == ProgramInterfaceKind::Graphical;
	}
};

struct ProgramSupervisorResult {
	ProgramSupervisorStatus status{ProgramSupervisorStatus::InvalidRequest};
	ProgramProcessId id{0};
	ProgramProcessState finalState{ProgramProcessState::Empty};
	ProgramExitResult exitResult{};

	[[nodiscard]] bool started() const {
		return status == ProgramSupervisorStatus::Started;
	}
};

using ProgramProcessVisitor = void (*)(ProgramProcessSnapshot const& process, void* userData);

class ProgramSupervisor final {
public:
	static constexpr std::size_t maxProcesses = lexxos::runtime::standardRuntimeProfile.programs.maxProcesses;
	static constexpr std::size_t maxArguments = lexxos::runtime::standardRuntimeProfile.programs.maxArguments;
	static constexpr std::size_t argumentStorageBytes =
	    lexxos::runtime::standardRuntimeProfile.programs.argumentStorageBytes;
	static constexpr std::uint32_t minimumTaskStackBytes =
	    lexxos::runtime::standardRuntimeProfile.programs.minimumTaskStackBytes;
	static constexpr std::uint32_t inlineTaskStackBytes =
	    lexxos::runtime::standardRuntimeProfile.programs.inlineTaskStackBytes;
	static constexpr std::uint32_t foregroundCleanupDelayMs =
	    lexxos::runtime::standardRuntimeProfile.programs.foregroundCleanupDelayMs;

	[[nodiscard]] ProgramSupervisorResult startForeground(
	    lexxos::os::AppContext& parentContext,
	    ProgramStartRequest const& request);

	[[nodiscard]] ProgramSupervisorResult startBackground(
	    lexxos::os::AppContext& parentContext,
	    ProgramStartRequest const& request);

	[[nodiscard]] bool requestStop(ProgramProcessId id);
	std::size_t clearFinished();
	void visitProcesses(ProgramProcessVisitor visitor, void* userData, bool& any) const;
	[[nodiscard]] ProgramFocusSnapshot focus();
	ProgramFocusSnapshot focusNext();
	[[nodiscard]] bool processFocused(ProgramProcessId id);
	void focusShell();
};

ProgramSupervisor& supervisor();

} // namespace lexxos::program
