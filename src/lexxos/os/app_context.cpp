#include <lexxos/os/app_context.h>

#include <array>
#include <cstring>

#include <lexxos/app/console_input.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/cancellation_token.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/shell/shell.h>

namespace lexxos::os {

AppContext::AppContext(OperatingSystem& os, AppContextCreateInfo const& info)
    : _os(&os)
    , _consoleOutput(os.platform())
    , _standardInput(info.standardInput != nullptr ? info.standardInput : &lexxos::io::nullInputStream())
    , _shell(info.shell)
    , _standardOutput(info.standardOutput)
    , _standardError(info.standardError)
    , _cancellationToken(info.cancellationToken)
    , _memoryArena(info.memoryArena)
    , _owner(info.owner)
    , _processId(info.processId) {
	setWorkingDirectory(info.workingDirectory);
}

AppContextOwner AppContext::owner() const {
	return _owner;
}

std::uint32_t AppContext::processId() const {
	return _processId;
}

bool AppContext::hasProgramFocus() const {
	return _processId != 0 && lexxos::program::supervisor().processFocused(_processId);
}

lexxos::shell::Shell* AppContext::shell() const {
	return _shell;
}

std::string_view AppContext::workingDirectory() const {
	return _workingDirectory.data();
}

bool AppContext::hasOperatingSystem() const {
	return _os != nullptr;
}

bool AppContext::setWorkingDirectory(std::string_view path) {
	if (path.empty() || path.size() + 1 > _workingDirectory.size()) {
		return false;
	}
	if (_shell != nullptr && !_shell->setWorkingDirectory(path)) {
		return false;
	}
	std::memcpy(_workingDirectory.data(), path.data(), path.size());
	_workingDirectory[path.size()] = '\0';
	return true;
}

AppContext AppContext::createChild(AppContextCreateInfo const& info) const {
	if (_os == nullptr) {
		return {};
	}

	return AppContext{
	    *_os,
	    {
	         .owner = info.owner,
	         .processId = info.processId != 0 ? info.processId : _processId,
	         .shell = info.shell != nullptr ? info.shell : _shell,
	         .workingDirectory = info.workingDirectory.empty() ? workingDirectory() : info.workingDirectory,
	         .standardInput = info.standardInput != nullptr ? info.standardInput : _standardInput,
	         .standardOutput = info.standardOutput != nullptr ? info.standardOutput : _standardOutput,
	         .standardError = info.standardError != nullptr ? info.standardError : _standardError,
	         .cancellationToken = info.cancellationToken != nullptr ? info.cancellationToken : _cancellationToken,
	         .memoryArena = info.memoryArena != nullptr ? info.memoryArena : _memoryArena,
	         }
    };
}

lexxos::io::InputStream& AppContext::standardInput() {
	return _standardInput != nullptr ? *_standardInput : lexxos::io::nullInputStream();
}

lexxos::io::OutputStream& AppContext::standardOutput() {
	return _standardOutput != nullptr ? *_standardOutput : _consoleOutput;
}

lexxos::io::OutputStream& AppContext::standardError() {
	return _standardError != nullptr ? *_standardError : _consoleOutput;
}

lexxos::devices::DeviceManager& AppContext::devices() {
	return _os->devices();
}

lexxos::vfs::Vfs& AppContext::vfs() {
	return _os->vfs();
}

lexxos::platform::PlatformServices const& AppContext::platform() const {
	return _os->platform();
}

lexxos::runtime::CapabilitySet AppContext::capabilities() const {
	return _os != nullptr ? _os->capabilities() : lexxos::runtime::CapabilitySet{};
}

lexxos::program::ProgramManager& AppContext::programs() {
	return _os->programs();
}

lexxos::program::ProgramManager const& AppContext::programs() const {
	return _os->programs();
}

lexxos::program::ProgramMemoryArena* AppContext::memoryArena() const {
	return _memoryArena;
}

lexxos::program::CancellationToken* AppContext::cancellationToken() const {
	return _cancellationToken;
}

bool AppContext::cancellationRequested() const {
	return _cancellationToken != nullptr && _cancellationToken->requested();
}

bool AppContext::pollCancellationRequest() {
	if (cancellationRequested()) {
		return true;
	}

	// A foreground program may continue running while the shell owns the focus.
	// Do not let an unfocused process drain console_input; otherwise Alt+Tab
	// switches input focus, but the background process can still consume shell keys.
	if (_owner == AppContextOwner::Program && _processId != 0 && !hasProgramFocus()) {
		return false;
	}

	if (_os == nullptr) {
		return false;
	}

	auto const& console = platform().console;
	if (console.readAvailable != nullptr) {
		std::array<char, 32> values{};
		for (;;) {
			auto const count = lexxos::app::console_input::readAvailable(platform(), values.data(), values.size());
			if (count == 0) {
				return false;
			}
			for (std::size_t index = 0; index < count; ++index) {
				if (static_cast<unsigned char>(values[index]) == 0x03U) {
					requestCancellation();
					return true;
				}
			}
			if (count < values.size()) {
				return false;
			}
		}
	}

	char value{};
	while (lexxos::app::console_input::readChar(platform(), value)) {
		if (static_cast<unsigned char>(value) == 0x03U) {
			requestCancellation();
			return true;
		}
	}
	return false;
}

void AppContext::requestCancellation() {
	if (_cancellationToken != nullptr) {
		_cancellationToken->request();
	}
}

void AppContext::resetCancellation() {
	if (_cancellationToken != nullptr) {
		_cancellationToken->reset();
	}
}

void AppContext::write(std::string_view text) {
	standardOutput().write(text);
}

void AppContext::writeLine(std::string_view text) {
	standardOutput().writeLine(text);
}

void AppContext::writeError(std::string_view text) {
	standardError().write(text);
}

void AppContext::writeErrorLine(std::string_view text) {
	standardError().writeLine(text);
}

lexxos::program::ProgramLaunchStatus AppContext::canLaunch(std::string_view programName) const {
	if (_os == nullptr) {
		return lexxos::program::ProgramLaunchStatus::MissingContext;
	}
	return _os->programs().availability(programName);
}

lexxos::program::ProgramExitResult AppContext::launch(lexxos::program::ProgramStartRequest const& request) {
	if (_os == nullptr) {
		return {
		    .launchStatus = lexxos::program::ProgramLaunchStatus::MissingContext,
		    .exitCode = 126,
		};
	}
	return _os->programs().execute(*this, request);
}

} // namespace lexxos::os
