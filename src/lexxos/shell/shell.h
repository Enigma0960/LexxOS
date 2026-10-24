#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/cancellation_token.h>
#include <lexxos/runtime/runtime_profile.h>
#include <lexxos/shell/command_registry.h>
#include <lexxos/shell/shell_escape_sequence_parser.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::program {
class ProgramManager;
} // namespace lexxos::program

namespace lexxos::os {
class OperatingSystem;
} // namespace lexxos::os

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::shell {

class Shell {
public:
	static constexpr std::size_t maxLineLength = lexxos::runtime::standardRuntimeProfile.shell.maxLineLength;
	static constexpr std::size_t maxArguments = lexxos::runtime::standardRuntimeProfile.shell.maxArguments;
	static constexpr std::size_t maxHistoryEntries = lexxos::runtime::standardRuntimeProfile.shell.maxHistoryEntries;
	static constexpr std::string_view configPath = "/mnt/sd/.lexx/shell.conf";

	~Shell();

	void registerCommand(Command command);
	void bindOperatingSystem(lexxos::os::OperatingSystem& operatingSystem);

	void printBanner();
	void printPrompt();
	void deferPrompt();
	void printPromptIfPending();
	void feed(char value);
	void feed(std::string_view text);
	void write(std::string_view text);
	void writeLine(std::string_view text);
	void requestStop();
	bool setWorkingDirectory(std::string_view path);

	[[nodiscard]] bool shouldStop() const;
	[[nodiscard]] std::string_view workingDirectory() const;
	[[nodiscard]] CommandRegistry const& commands() const;
	[[nodiscard]] bool autosuggestEnabled() const;
	[[nodiscard]] std::size_t historySize() const;
	[[nodiscard]] bool shellConfigLoaded() const;
	void setAutosuggestEnabled(bool enabled);
	bool loadShellConfig(bool force = false);
	bool saveShellConfig();

private:
	void _executeLine();
	void _printOrDeferPrompt();
	void _writePrompt();
	void _activateInputLineGuard();
	void _deactivateInputLineGuard();
	void _clearInputLineForExternalConsoleWrite();
	void _redrawInputLineAfterExternalConsoleWrite();
	static void _clearInputLineForExternalConsoleWriteCallback(void* context);
	static void _redrawInputLineAfterExternalConsoleWriteCallback(void* context);
	void _resetLine();
	void _putChar(char value);
	void _eraseLastChar();
	void _eraseCurrentChar();
	void _handleInputKey(detail::ShellInputKey key);
	[[nodiscard]] bool _historyLineAllowed(std::string_view text) const;
	void _addHistoryEntry();
	void _clearHistory();
	void _storeHistoryEntry(std::string_view text);
	void _loadHistoryEntry(std::size_t offset);
	void _showPreviousHistoryEntry();
	void _showNextHistoryEntry();
	void _replaceInputLine(char const* text, std::size_t length);
	void _saveHistoryDraft();
	void _stopHistoryBrowsing();
	bool _acceptAutosuggestion();
	[[nodiscard]] bool _shouldRedrawForAutosuggestion() const;
	[[nodiscard]] std::string_view _autosuggestionSuffix() const;
	void _completeInputLine();
	void _completeProgramName(std::size_t tokenStart, std::size_t tokenEnd);
	void _completePath(std::size_t tokenStart, std::size_t tokenEnd);
	void _insertText(std::string_view text);
	void _replaceInputRange(std::size_t start, std::size_t end, std::string_view text);
	void _writeProgramCompletionMatches(std::string_view prefix);
	void _writePathCompletionMatches(std::string_view directoryPath, std::string_view prefix);
	[[nodiscard]] lexxos::platform::PlatformServices const& _platformServices() const;
	[[nodiscard]] lexxos::vfs::Vfs* _vfs() const;
	[[nodiscard]] bool _shellConfigStorageReady() const;
	[[nodiscard]] bool _ensureShellConfigDirectory();
	void _loadShellConfigIfAvailable();
	void _markShellConfigDirty();
	void _trySaveShellConfigIfDirty();
	bool _writeShellConfig();
	bool _readShellConfig(bool loadHistory);
	bool _parseShellConfigLine(std::string_view line, bool loadHistory, std::array<char, maxLineLength>& decoded);
	[[nodiscard]] bool _lineIsBlank() const;
	[[nodiscard]] bool _matchesLastHistoryEntry(std::string_view text) const;
	[[nodiscard]] bool _matchesLastHistoryEntry() const;
	[[nodiscard]] bool _findCompletionToken(std::size_t& tokenStart, std::size_t& tokenEnd, bool& programToken) const;

	void _redrawInputLine();
	void _moveCursorLeft();
	void _moveCursorRight();
	void _moveCursorHome();
	void _moveCursorEnd();
	void _moveTerminalCursorLeft(std::size_t count);

	CommandRegistry _commands;
	std::array<char, maxLineLength> _lineBuffer{};
	std::array<char, maxLineLength> _executionLine{};
	std::array<char const*, maxArguments> _argv{};
	std::array<char const*, maxArguments> _commandArgv{};
	std::array<std::array<char, maxLineLength>, maxHistoryEntries> _history{};
	std::array<std::size_t, maxHistoryEntries> _historyLengths{};
	std::array<char, maxLineLength> _historyDraft{};
	std::array<char, lexxos::vfs::maxPathLength> _workingDirectory{'/', '\0'};
	std::size_t _historyDraftLength{0};
	std::size_t _historyWriteIndex{0};
	std::size_t _historySize{0};
	std::size_t _historyBrowseOffset{0};
	bool _historyBrowsing{false};
	std::size_t _lineLength{0};
	std::size_t _cursorPosition{0};
	bool _autosuggestEnabled{true};
	bool _autosuggestionVisible{false};
	bool _shellConfigLoaded{false};
	bool _shellConfigDirty{false};
	bool _stopRequested{false};
	bool _previousWasCarriageReturn{false};
	detail::ShellEscapeSequenceParser _escapeParser;
	lexxos::program::CancellationToken _programCancellation;
	lexxos::os::OperatingSystem* _operatingSystem{nullptr};
	bool _programRunning{false};
	bool _lastOutputWasCarriageReturn{false};
	bool _promptPending{false};
	bool _inputLineGuardActive{false};
};

} // namespace lexxos::shell
