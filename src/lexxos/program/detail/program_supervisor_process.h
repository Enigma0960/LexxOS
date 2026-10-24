#pragma once

#include <array>
#include <atomic>
#include <cstring>

#include <lexxos/os/app_context.h>
#include <lexxos/program/cancellation_token.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::program::detail {

struct OwnedProgramArguments {
	std::array<char, ProgramSupervisor::argumentStorageBytes> storage{};
	std::array<char const*, ProgramSupervisor::maxArguments> argv{};
	int argc{0};

	void clear() {
		storage.fill('\0');
		argv.fill(nullptr);
		argc = 0;
	}

	[[nodiscard]] bool assign(int sourceArgc, char const* const* sourceArgv) {
		clear();
		if (sourceArgc <= 0 || sourceArgv == nullptr || sourceArgc > static_cast<int>(argv.size())) {
			return false;
		}

		std::size_t offset = 0;
		for (int index = 0; index < sourceArgc; ++index) {
			if (sourceArgv[index] == nullptr) {
				return false;
			}

			auto const length = std::strlen(sourceArgv[index]);
			if (offset + length + 1 > storage.size()) {
				return false;
			}

			char* const target = storage.data() + offset;
			std::memcpy(target, sourceArgv[index], length + 1);
			argv[static_cast<std::size_t>(index)] = target;
			offset += length + 1;
		}
		argc = sourceArgc;
		return true;
	}
};

struct ProgramProcessSlot {
	std::atomic<ProgramProcessState> state{ProgramProcessState::Empty};
	ProgramProcessId id{0};
	OwnedProgramArguments arguments{};
	ProgramStartRequest request{};
	ProgramRuntimeProfile runtime{};
	ProgramMemoryArena memoryArena{};
	ProgramExitResult result{};
	std::uint32_t stackFreeBytes{0};
	CancellationToken cancellation{};
	lexxos::os::AppContext context{};
};

} // namespace lexxos::program::detail
