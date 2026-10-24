#include <lexxos/shell/shell.h>

#include <array>
#include <cstdint>
#include <cstring>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/console_output.h>
#include <lexxos/app/program_input.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/shell/shell_redirection_parser.h>
#include <lexxos/vfs/vfs.h>

namespace {

using lexxos::shell::detail::applyRedirections;
using lexxos::shell::detail::RedirectionParseResult;
using lexxos::shell::detail::RedirectionParseStatus;
using lexxos::shell::detail::ShellRedirection;

// @formatter:off
constexpr char const* kBootLogo = R"(
 _                     ___  ____
| |    _____  ____  __/ _ \/ ___|
| |   / _ \ \/ /\ \/ / | | \___ \
| |__|  __/>  <  >  <| |_| |___) |
|_____\___/_/\_\/_/\_\\___/|____/

     organic kernel console
)";

// @formatter:on

constexpr char const* kLexxRunProgramName = "lexx-run";
constexpr std::string_view kLexxExecutableSuffix{".lexx"};

bool isSpace(char value) {
	return value == ' ' || value == '\t';
}

bool endsWith(std::string_view value, std::string_view suffix) {
	return value.size() >= suffix.size()
	    && value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

bool isLexxExecutableToken(std::string_view token) {
	return endsWith(token, kLexxExecutableSuffix);
}

std::uint32_t timestampMs(lexxos::platform::PlatformServices const& platform) {
	return platform.clock.uptimeMs != nullptr ? platform.clock.uptimeMs() : 0;
}

lexxos::devices::KeyCode keyCodeFromUartChar(char value) {
	using lexxos::devices::KeyCode;
	switch (value) {
		case 0x03:
			return KeyCode::C;
		case 'q':
		case 'Q':
			return KeyCode::Q;
		case 'w':
		case 'W':
		case 'k':
		case 'K':
			return KeyCode::Up;
		case 's':
		case 'S':
		case 'j':
		case 'J':
			return KeyCode::Down;
		case 'h':
		case 'H':
			return KeyCode::Left;
		case 'l':
		case 'L':
			return KeyCode::Right;
		case 'a':
		case 'A':
			return KeyCode::A;
		case 'b':
		case 'B':
			return KeyCode::B;
		case 'c':
		case 'C':
			return KeyCode::C;
		case 'x':
		case 'X':
			return KeyCode::X;
		case 'z':
		case 'Z':
			return KeyCode::Z;
		case '1':
			return KeyCode::Num1;
		case '3':
			return KeyCode::Num3;
		case '\t':
			return KeyCode::Tab;
		case ' ':
			return KeyCode::Space;
		case '\r':
		case '\n':
			return KeyCode::Enter;
		case '\b':
		case 0x7F:
			return KeyCode::Backspace;
		default:
			return KeyCode::Unknown;
	}
}

lexxos::devices::KeyCode keyCodeFromShellInputKey(lexxos::shell::detail::ShellInputKey key) {
	using lexxos::devices::KeyCode;
	switch (key) {
		case lexxos::shell::detail::ShellInputKey::ArrowUp:
			return KeyCode::Up;
		case lexxos::shell::detail::ShellInputKey::ArrowDown:
			return KeyCode::Down;
		case lexxos::shell::detail::ShellInputKey::ArrowRight:
			return KeyCode::Right;
		case lexxos::shell::detail::ShellInputKey::ArrowLeft:
			return KeyCode::Left;
		case lexxos::shell::detail::ShellInputKey::Home:
			return KeyCode::Home;
		case lexxos::shell::detail::ShellInputKey::End:
			return KeyCode::End;
		case lexxos::shell::detail::ShellInputKey::Delete:
			return KeyCode::Delete;
	}
	return KeyCode::Unknown;
}

void publishUartInputToFocusedProgram(
    lexxos::devices::KeyCode key,
    char character,
    std::uint8_t modifiers,
    lexxos::platform::PlatformServices const& platform) {
	lexxos::devices::InputEvent event{
	    .type = lexxos::devices::InputEventType::KeyDown,
	    .key = key,
	    .timestampMs = timestampMs(platform),
	    .character = static_cast<std::uint8_t>(character),
	    .scanCode = 0,
	    .modifiers = modifiers,
	};
	lexxos::app::program_input::publish(event);
}

void injectFocusedProgramConsoleByte(char value) {
	lexxos::app::console_input::inject(std::string_view{&value, 1});
}

void forwardUartInputToFocusedProgram(char value, lexxos::platform::PlatformServices const& platform) {
	injectFocusedProgramConsoleByte(value);
	publishUartInputToFocusedProgram(
	    keyCodeFromUartChar(value),
	    value,
	    static_cast<std::uint8_t>(static_cast<unsigned char>(value) == 0x03U ? lexxos::devices::InputModifierCtrl : 0U),
	    platform);
}

// The shell command line grammar is currently ASCII-only.
// Some USB-UART adapters can leave a stray high-bit byte in RX FIFO after target reset.
// Do not echo/store such bytes, otherwise the first prompt may become an invalid command.
bool isCommandLineCharacter(char value) {
	auto const byte = static_cast<unsigned char>(value);
	return byte >= 0x20U && byte <= 0x7EU;
}

void writeLaunchError(lexxos::io::OutputStream& stream, std::string_view programName, lexxos::program::ProgramLaunchStatus status) {
	if (status == lexxos::program::ProgramLaunchStatus::NotFound) {
		stream.write("unknown program: ");
		stream.writeLine(programName);
		return;
	}

	stream.write("cannot launch program: ");
	stream.write(programName);
	stream.write(": ");
	stream.writeLine(lexxos::program::toString(status));
}

void writeRedirectionError(lexxos::shell::Shell& shell, RedirectionParseResult const& result) {
	switch (result.status) {
		case RedirectionParseStatus::Ok:
			return;
		case RedirectionParseStatus::MissingTarget:
			shell.write("redirection: missing target after ");
			shell.writeLine(result.token);
			return;
		case RedirectionParseStatus::InvalidTarget:
			shell.write("redirection: invalid target: ");
			shell.writeLine(result.token);
			return;
		case RedirectionParseStatus::UnexpectedTarget:
			shell.write("redirection: invalid token: ");
			shell.writeLine(result.token);
			return;
	}
}

void applyLaunchFocusToConsole() {
	auto const focus = lexxos::program::supervisor().focus();
	lexxos::app::console_input::setMode(
	    focus.programFocused() ? lexxos::app::console_input::Mode::Program : lexxos::app::console_input::Mode::Shell);

	if (focus.graphicalProgramFocused()) {
		// The foreground task has already been selected by ProgramSupervisor.
		// Switch the display mirror immediately, otherwise the shell can keep
		// the text console visible until the next Alt+Tab/input polling cycle.
		lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Graphics);
		return;
	}

	lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
	lexxos::app::console_mirror::refresh();
}

} // namespace

namespace lexxos::shell {

Shell::~Shell() {
	_deactivateInputLineGuard();
}

void Shell::registerCommand(Command command) {
	if (!_commands.add(command)) {
		write("failed to register command: ");
		write(command.name);
		writeLine("");
	}
}

void Shell::bindOperatingSystem(lexxos::os::OperatingSystem& operatingSystem) {
	_operatingSystem = &operatingSystem;
}

void Shell::printBanner() {
	auto const& board = _platformServices().board;
	auto const& descriptor = board.current != nullptr ? board.current() : lexxos::platform::board();

	writeLine(kBootLogo);

	writeLine("");
	writeLine("LexxOS shell");
	write("board: ");
	writeLine(descriptor.boardName);
	write("cpu: ");
	write(descriptor.processorName);
	write(" (");
	write(lexxos::platform::toString(descriptor.processorFamily));
	writeLine(")");
	writeLine("type 'help' to list programs");
	writeLine("");
}

void Shell::printPrompt() {
	_loadShellConfigIfAvailable();
	_trySaveShellConfigIfDirty();
	_writePrompt();
}

void Shell::_writePrompt() {
	_promptPending = false;
	write("lexx:");
	write(workingDirectory());
	write("> ");
	_activateInputLineGuard();
}

void Shell::deferPrompt() {
	_promptPending = true;
	_deactivateInputLineGuard();
}

void Shell::printPromptIfPending() {
	if (!_promptPending) {
		return;
	}

	printPrompt();
}

void Shell::feed(char value) {
	auto const focus = lexxos::program::supervisor().focus();
	if (focus.programFocused()) {
		_deactivateInputLineGuard();
		_previousWasCarriageReturn = false;
		if (_escapeParser.active()) {
			injectFocusedProgramConsoleByte(value);
			auto const result = _escapeParser.consume(value);
			if (result.hasKey) {
				publishUartInputToFocusedProgram(keyCodeFromShellInputKey(result.key), 0, 0, _platformServices());
			}
			return;
		}
		if (static_cast<unsigned char>(value) == 0x1BU) {
			injectFocusedProgramConsoleByte(value);
			_escapeParser.start();
			return;
		}
		auto const inputMode = lexxos::app::console_input::mode();
		if (static_cast<unsigned char>(value) == 0x03U
		    && inputMode != lexxos::app::console_input::Mode::RawProgram) {
			publishUartInputToFocusedProgram(
			    lexxos::devices::KeyCode::C,
			    value,
			    lexxos::devices::InputModifierCtrl,
			    _platformServices());
			(void) lexxos::program::supervisor().requestStop(focus.processId);
			writeLine("^C");
			return;
		}
		forwardUartInputToFocusedProgram(value, _platformServices());
		return;
	}

	if (static_cast<unsigned char>(value) == 0x03U) {
		_deactivateInputLineGuard();
		_previousWasCarriageReturn = false;
		_escapeParser.reset();
		_programCancellation.request();
		if (_autosuggestionVisible) {
			write("\x1B[K");
			_autosuggestionVisible = false;
		}
		writeLine("^C");
		if (!_programRunning) {
			_resetLine();
			if (!shouldStop()) {
				printPrompt();
			}
		}
		return;
	}

	if (static_cast<unsigned char>(value) == 0x06U) {
		_previousWasCarriageReturn = false;
		if (!_acceptAutosuggestion()) {
			write("\a");
		}
		return;
	}

	if (_escapeParser.active()) {
		auto const result = _escapeParser.consume(value);
		if (result.hasKey) {
			_handleInputKey(result.key);
		}
		return;
	}

	if (static_cast<unsigned char>(value) == 0x1BU) {
		_previousWasCarriageReturn = false;
		_escapeParser.start();
		return;
	}

	if (value == '\r') {
		_deactivateInputLineGuard();
		_previousWasCarriageReturn = true;
		_escapeParser.reset();
		if (_autosuggestionVisible) {
			write("\x1B[K");
			_autosuggestionVisible = false;
		}
		writeLine("");
		_executeLine();
		if (!shouldStop()) {
			_printOrDeferPrompt();
		}
		return;
	}

	if (value == '\n') {
		if (_previousWasCarriageReturn) {
			_previousWasCarriageReturn = false;
			return;
		}

		_deactivateInputLineGuard();
		_escapeParser.reset();
		if (_autosuggestionVisible) {
			write("\x1B[K");
			_autosuggestionVisible = false;
		}
		writeLine("");
		_executeLine();
		if (!shouldStop()) {
			_printOrDeferPrompt();
		}
		return;
	}

	_previousWasCarriageReturn = false;

	if (value == '\b' || value == 0x7F) {
		_eraseLastChar();
		return;
	}

	if (value == '\t') {
		_completeInputLine();
		return;
	}

	if (!isCommandLineCharacter(value)) {
		return;
	}

	_putChar(value);
}

void Shell::feed(std::string_view text) {
	std::size_t index = 0;
	while (index < text.size()) {
		auto const focus = lexxos::program::supervisor().focus();
		if (!focus.programFocused() && !_escapeParser.active()) {
			auto const start = index;
			while (index < text.size() && isCommandLineCharacter(text[index])) {
				++index;
			}
			if (index > start) {
				_previousWasCarriageReturn = false;
				auto const run = text.substr(start, index - start);
				auto const available = _lineLength < _lineBuffer.size() ? _lineBuffer.size() - _lineLength - 1U : 0U;
				if (run.size() <= available) {
					_insertText(run);
				} else {
					if (available != 0) {
						_insertText(run.substr(0, available));
					}
					write("\a");
				}
				continue;
			}
		}

		feed(text[index]);
		++index;
	}
}

void Shell::write(std::string_view text) {
	lexxos::app::console_output::ScopedInteractiveLineGuardBypass bypass;
	lexxos::app::console_output::writeNormalized(_platformServices(), text, _lastOutputWasCarriageReturn);
}

void Shell::writeLine(std::string_view text) {
	write(text);
#if __APPLE__
	write("\n");
#else
	write("\r\n");
#endif
}

void Shell::requestStop() {
	_deactivateInputLineGuard();
	_stopRequested = true;
}

bool Shell::setWorkingDirectory(std::string_view path) {
	if (path.empty() || path.size() + 1 > _workingDirectory.size()) {
		return false;
	}
	std::memcpy(_workingDirectory.data(), path.data(), path.size());
	_workingDirectory[path.size()] = '\0';
	return true;
}

bool Shell::shouldStop() const {
	return _stopRequested;
}

std::string_view Shell::workingDirectory() const {
	return _workingDirectory.data();
}

CommandRegistry const& Shell::commands() const {
	return _commands;
}

lexxos::platform::PlatformServices const& Shell::_platformServices() const {
	return _operatingSystem != nullptr ? _operatingSystem->platform() : lexxos::platform::services();
}

void Shell::_activateInputLineGuard() {
	lexxos::app::console_output::setInteractiveLineGuard({
	    .context = this,
	    .clear = _clearInputLineForExternalConsoleWriteCallback,
	    .redraw = _redrawInputLineAfterExternalConsoleWriteCallback,
	});
	_inputLineGuardActive = true;
}

void Shell::_deactivateInputLineGuard() {
	if (!_inputLineGuardActive) {
		return;
	}

	lexxos::app::console_output::clearInteractiveLineGuard(this);
	_inputLineGuardActive = false;
}

void Shell::_clearInputLineForExternalConsoleWrite() {
	if (!_inputLineGuardActive) {
		return;
	}

	write("\r\x1B[K");
}

void Shell::_redrawInputLineAfterExternalConsoleWrite() {
	if (!_inputLineGuardActive || _programRunning || _promptPending) {
		return;
	}

	_redrawInputLine();
}

void Shell::_clearInputLineForExternalConsoleWriteCallback(void* context) {
	if (context == nullptr) {
		return;
	}

	static_cast<Shell*>(context)->_clearInputLineForExternalConsoleWrite();
}

void Shell::_redrawInputLineAfterExternalConsoleWriteCallback(void* context) {
	if (context == nullptr) {
		return;
	}

	static_cast<Shell*>(context)->_redrawInputLineAfterExternalConsoleWrite();
}

void Shell::_printOrDeferPrompt() {
	if (lexxos::program::supervisor().focus().programFocused()) {
		deferPrompt();
		return;
	}

	printPrompt();
}

void Shell::_executeLine() {
	_lineBuffer[_lineLength] = '\0';
	_addHistoryEntry();
	_stopHistoryBrowsing();

	std::memcpy(_executionLine.data(), _lineBuffer.data(), _lineLength + 1);
	_resetLine();

	int argc = 0;
	char* cursor = _executionLine.data();

	while (*cursor != '\0') {
		while (isSpace(*cursor)) {
			++cursor;
		}

		if (*cursor == '\0') {
			break;
		}

		if (argc >= static_cast<int>(_argv.size())) {
			writeLine("too many arguments");
			return;
		}

		char* const argumentStart = cursor;
		char* writer = cursor;
		char quote = 0;
		bool closedQuote = true;

		_argv[static_cast<std::size_t>(argc)] = argumentStart;
		++argc;

		while (*cursor != '\0') {
			if (quote != 0) {
				if (*cursor == quote) {
					quote = 0;
					closedQuote = true;
					++cursor;
					continue;
				}
				if (quote == '"' && *cursor == '\\' && cursor[1] != '\0') {
					++cursor;
				}
				*writer++ = *cursor++;
				continue;
			}

			if (isSpace(*cursor)) {
				break;
			}
			if (*cursor == '"' || *cursor == '\'') {
				quote = *cursor;
				closedQuote = false;
				++cursor;
				continue;
			}
			if (*cursor == '\\' && cursor[1] != '\0') {
				++cursor;
			}
			*writer++ = *cursor++;
		}

		if (!closedQuote) {
			writeLine("unterminated quoted argument");
			return;
		}

		if (*cursor == '\0') {
			*writer = '\0';
			break;
		}

		*writer = '\0';
		++cursor;
	}

	if (argc == 0) {
		return;
	}

	int commandArgc = 0;
	ShellRedirection redirection{};
	auto const redirectionResult = applyRedirections(_argv, argc, _commandArgv, commandArgc, redirection, workingDirectory());
	if (redirectionResult.status != RedirectionParseStatus::Ok) {
		writeRedirectionError(*this, redirectionResult);
		return;
	}
	if (commandArgc == 0) {
		return;
	}

	if (isLexxExecutableToken(_commandArgv[0])) {
		if (commandArgc >= static_cast<int>(_commandArgv.size())) {
			writeLine("too many arguments");
			return;
		}
		for (int index = commandArgc; index > 0; --index) {
			_commandArgv[static_cast<std::size_t>(index)] = _commandArgv[static_cast<std::size_t>(index - 1)];
		}
		_commandArgv[0] = kLexxRunProgramName;
		++commandArgc;
	}

	if (_operatingSystem == nullptr) {
		writeLine("shell: missing operating system context");
		return;
	}

	auto context = _operatingSystem->createAppContext({
	    .owner = lexxos::os::AppContextOwner::Shell,
	    .shell = this,
	    .workingDirectory = workingDirectory(),
	});

	lexxos::io::FileInputStream standardInputFile;
	lexxos::io::FileOutputStream standardOutputFile;
	lexxos::io::FileOutputStream standardErrorFile;

	if (redirection.standardInputFile) {
		standardInputFile.open(redirection.standardInputPath.data());
		if (!standardInputFile.ok()) {
			write("redirection: cannot open ");
			write(redirection.standardInputPath.data());
			write(": ");
			writeLine(lexxos::vfs::toString(standardInputFile.status()));
			return;
		}
		redirection.standardInput = &standardInputFile;
	}

	if (redirection.standardOutputFile) {
		standardOutputFile.open(redirection.standardOutputPath.data(), redirection.standardOutputMode);
		if (!standardOutputFile.ok()) {
			write("redirection: cannot open ");
			write(standardOutputFile.path());
			write(": ");
			writeLine(lexxos::vfs::toString(standardOutputFile.status()));
			return;
		}
		redirection.standardOutput = &standardOutputFile;
	}

	if (redirection.standardErrorFile) {
		standardErrorFile.open(redirection.standardErrorPath.data(), redirection.standardErrorMode);
		if (!standardErrorFile.ok()) {
			write("redirection: cannot open ");
			write(standardErrorFile.path());
			write(": ");
			writeLine(lexxos::vfs::toString(standardErrorFile.status()));
			return;
		}
		redirection.standardError = &standardErrorFile;
	}

	lexxos::program::ProgramStartRequest request{
	    .name = _commandArgv[0],
	    .argc = commandArgc,
	    .argv = _commandArgv.data(),
	    .standardInput = redirection.standardInput,
	    .standardOutput = redirection.standardOutput,
	    .standardError = redirection.standardError,
	    .cancellationToken = &_programCancellation,
	};

	_programCancellation.reset();
	lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::Program);
	_deactivateInputLineGuard();
	_programRunning = true;
	auto const result = context.launch(request);
	_programRunning = false;

	applyLaunchFocusToConsole();

	if (result.launchStatus == lexxos::program::ProgramLaunchStatus::NotFound
	    || (result.launchStatus != lexxos::program::ProgramLaunchStatus::Available
	        && result.launchStatus != lexxos::program::ProgramLaunchStatus::InvalidArguments)) {
		auto& errorStream = redirection.standardError != nullptr ? *redirection.standardError : lexxos::io::consoleOutputStream();
		writeLaunchError(errorStream, _commandArgv[0], result.launchStatus);
	}
}

} // namespace lexxos::shell
