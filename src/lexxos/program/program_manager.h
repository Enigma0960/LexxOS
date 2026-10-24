#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/program.h>
#include <lexxos/runtime/runtime_profile.h>
#include <lexxos/shell/command.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

using ProgramVisitor = bool (*)(ProgramDescriptor const& descriptor, void* userData);

class ProgramManager {
public:
	static constexpr std::size_t maxPrograms = lexxos::runtime::standardRuntimeProfile.programs.maxPrograms;

	bool registerProgram(ProgramDescriptor descriptor);
	bool registerLegacyCommand(lexxos::shell::Command command);

	[[nodiscard]] ProgramDescriptor const* find(std::string_view name) const;
	[[nodiscard]] ProgramLaunchStatus availability(std::string_view name) const;
	ProgramExitResult execute(lexxos::os::AppContext& context, ProgramStartRequest const& request);
	void writeProgramList(lexxos::os::AppContext& context) const;
	void writeProgramHelp(lexxos::os::AppContext& context, ProgramDescriptor const& descriptor) const;

	bool visitPrograms(ProgramVisitor visitor, void* userData) const;
	[[nodiscard]] std::size_t size() const;
	[[nodiscard]] ProgramDescriptor const& at(std::size_t index) const;

private:
	enum class ProgramKind : std::uint8_t {
		Native,
		LegacyCommand,
	};

	struct Entry {
		ProgramDescriptor descriptor{};
		lexxos::shell::Command legacyCommand{};
		ProgramKind kind{ProgramKind::Native};
	};

	Entry const* _findEntry(std::string_view name) const;
	Entry* _findEntry(std::string_view name);

	std::array<Entry, maxPrograms> _programs{};
	std::size_t _size{0};
};

} // namespace lexxos::program
