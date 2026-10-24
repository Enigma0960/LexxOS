#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/io/output_stream.h>
#include <lexxos/program/program.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::program {
class ProgramManager;
class ProgramMemoryArena;
} // namespace lexxos::program

namespace lexxos::io {
class InputStream;
} // namespace lexxos::io

namespace lexxos::devices {
class DeviceManager;
} // namespace lexxos::devices

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::shell {

class Shell;

} // namespace lexxos::shell

namespace lexxos::os {

class OperatingSystem;

enum class AppContextOwner : std::uint8_t {
	Kernel,
	Shell,
	Program,
};

struct AppContextCreateInfo {
	AppContextOwner owner{AppContextOwner::Program};
	std::uint32_t processId{0};
	lexxos::shell::Shell* shell{nullptr};
	std::string_view workingDirectory{"/"};
	lexxos::io::InputStream* standardInput{nullptr};
	lexxos::io::OutputStream* standardOutput{nullptr};
	lexxos::io::OutputStream* standardError{nullptr};
	lexxos::program::CancellationToken* cancellationToken{nullptr};
	lexxos::program::ProgramMemoryArena* memoryArena{nullptr};
};

class AppContext {
public:
	AppContext() = default;
	AppContext(OperatingSystem& os, AppContextCreateInfo const& info);

	[[nodiscard]] AppContextOwner owner() const;
	[[nodiscard]] std::uint32_t processId() const;
	[[nodiscard]] bool hasProgramFocus() const;
	[[nodiscard]] lexxos::shell::Shell* shell() const;
	[[nodiscard]] std::string_view workingDirectory() const;
	[[nodiscard]] bool hasOperatingSystem() const;

	bool setWorkingDirectory(std::string_view path);

	[[nodiscard]] AppContext createChild(AppContextCreateInfo const& info) const;

	[[nodiscard]] lexxos::io::InputStream& standardInput();
	[[nodiscard]] lexxos::io::OutputStream& standardOutput();
	[[nodiscard]] lexxos::io::OutputStream& standardError();
	[[nodiscard]] lexxos::devices::DeviceManager& devices();
	[[nodiscard]] lexxos::vfs::Vfs& vfs();
	[[nodiscard]] lexxos::platform::PlatformServices const& platform() const;
	[[nodiscard]] lexxos::runtime::CapabilitySet capabilities() const;
	[[nodiscard]] lexxos::program::ProgramManager& programs();
	[[nodiscard]] lexxos::program::ProgramManager const& programs() const;
	[[nodiscard]] lexxos::program::ProgramMemoryArena* memoryArena() const;
	[[nodiscard]] lexxos::program::CancellationToken* cancellationToken() const;
	[[nodiscard]] bool cancellationRequested() const;
	[[nodiscard]] bool pollCancellationRequest();
	void requestCancellation();
	void resetCancellation();

	void write(std::string_view text);
	void writeLine(std::string_view text);
	void writeError(std::string_view text);
	void writeErrorLine(std::string_view text);

	[[nodiscard]] lexxos::program::ProgramLaunchStatus canLaunch(std::string_view programName) const;
	lexxos::program::ProgramExitResult launch(lexxos::program::ProgramStartRequest const& request);

private:
	OperatingSystem* _os{nullptr};
	lexxos::io::ConsoleOutputStream _consoleOutput{};
	lexxos::io::InputStream* _standardInput{nullptr};
	lexxos::shell::Shell* _shell{nullptr};
	lexxos::io::OutputStream* _standardOutput{nullptr};
	lexxos::io::OutputStream* _standardError{nullptr};
	lexxos::program::CancellationToken* _cancellationToken{nullptr};
	lexxos::program::ProgramMemoryArena* _memoryArena{nullptr};
	AppContextOwner _owner{AppContextOwner::Program};
	std::uint32_t _processId{0};
	std::array<char, lexxos::vfs::maxPathLength> _workingDirectory{'/', '\0'};
};

} // namespace lexxos::os
