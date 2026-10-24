#include <lexxos/shell/shell.h>

#include <lexxos/os/operating_system.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/shell/shell_history_policy.h>

#include <cstring>
#include <string_view>

namespace {

bool isSpace(char value) {
	return value == ' ' || value == '\t';
}

} // namespace

namespace lexxos::shell {

void Shell::_resetLine() {
	_lineLength = 0;
	_cursorPosition = 0;
	_lineBuffer[0] = '\0';
	_autosuggestionVisible = false;
	_escapeParser.reset();
}

void Shell::_putChar(char value) {
	if (_lineLength + 1 >= _lineBuffer.size()) {
		write("\a");
		return;
	}

	_stopHistoryBrowsing();

	if (_cursorPosition < _lineLength) {
		auto const tailLength = _lineLength - _cursorPosition;
		std::memmove(_lineBuffer.data() + _cursorPosition + 1, _lineBuffer.data() + _cursorPosition, tailLength);
		_lineBuffer[_cursorPosition] = value;
		++_lineLength;
		++_cursorPosition;
		_lineBuffer[_lineLength] = '\0';
		_redrawInputLine();
		return;
	}

	_lineBuffer[_lineLength] = value;
	++_lineLength;
	++_cursorPosition;
	_lineBuffer[_lineLength] = '\0';
	if (_shouldRedrawForAutosuggestion()) {
		_redrawInputLine();
		return;
	}
	write(std::string_view(&value, 1));
}

void Shell::_eraseLastChar() {
	if (_cursorPosition == 0 || _lineLength == 0) {
		return;
	}

	_stopHistoryBrowsing();

	auto const erasePosition = _cursorPosition - 1;
	if (erasePosition + 1 < _lineLength) {
		auto const tailLength = _lineLength - erasePosition - 1;
		std::memmove(_lineBuffer.data() + erasePosition, _lineBuffer.data() + erasePosition + 1, tailLength);
	}

	--_lineLength;
	--_cursorPosition;
	_lineBuffer[_lineLength] = '\0';

	if (_cursorPosition == _lineLength) {
		if (_shouldRedrawForAutosuggestion()) {
			_redrawInputLine();
			return;
		}
		write("\b \b");
		return;
	}
	_redrawInputLine();
}

void Shell::_eraseCurrentChar() {
	if (_cursorPosition >= _lineLength) {
		return;
	}

	_stopHistoryBrowsing();

	if (_cursorPosition + 1 < _lineLength) {
		auto const tailLength = _lineLength - _cursorPosition - 1;
		std::memmove(_lineBuffer.data() + _cursorPosition, _lineBuffer.data() + _cursorPosition + 1, tailLength);
	}
	--_lineLength;
	_lineBuffer[_lineLength] = '\0';
	_redrawInputLine();
}

void Shell::_insertText(std::string_view text) {
	if (text.empty()) {
		return;
	}
	if (_lineLength + text.size() >= _lineBuffer.size()) {
		write("\a");
		return;
	}

	_stopHistoryBrowsing();

	if (_cursorPosition < _lineLength) {
		auto const tailLength = _lineLength - _cursorPosition;
		std::memmove(_lineBuffer.data() + _cursorPosition + text.size(), _lineBuffer.data() + _cursorPosition, tailLength);
		std::memcpy(_lineBuffer.data() + _cursorPosition, text.data(), text.size());
		_lineLength += text.size();
		_cursorPosition += text.size();
		_lineBuffer[_lineLength] = '\0';
		_redrawInputLine();
		return;
	}

	std::memcpy(_lineBuffer.data() + _lineLength, text.data(), text.size());
	_lineLength += text.size();
	_cursorPosition += text.size();
	_lineBuffer[_lineLength] = '\0';
	if (_shouldRedrawForAutosuggestion()) {
		_redrawInputLine();
		return;
	}
	write(text);
}

void Shell::_replaceInputRange(std::size_t start, std::size_t end, std::string_view text) {
	if (start > end || end > _lineLength) {
		write("\a");
		return;
	}

	auto const rangeLength = end - start;
	auto const newLength = _lineLength - rangeLength + text.size();
	if (newLength >= _lineBuffer.size()) {
		write("\a");
		return;
	}

	_stopHistoryBrowsing();

	auto const tailLength = _lineLength - end;
	if (text.size() != rangeLength && tailLength != 0) {
		std::memmove(_lineBuffer.data() + start + text.size(), _lineBuffer.data() + end, tailLength);
	}
	if (!text.empty()) {
		std::memcpy(_lineBuffer.data() + start, text.data(), text.size());
	}

	_lineLength = newLength;
	_cursorPosition = start + text.size();
	_lineBuffer[_lineLength] = '\0';
	_redrawInputLine();
}

bool Shell::_historyLineAllowed(std::string_view text) const {
	auto lookup = [](std::string_view name, void const* context) {
		if (context == nullptr) {
			return false;
		}
		auto const* os = static_cast<lexxos::os::OperatingSystem const*>(context);
		auto const* descriptor = os->programs().find(name);
		return descriptor != nullptr && detail::hasSensitiveArguments(descriptor->arguments);
	};
	return detail::historyLineAllowed(text, lookup, _operatingSystem);
}

void Shell::_addHistoryEntry() {
	if (_lineIsBlank() || _matchesLastHistoryEntry() || !_historyLineAllowed(std::string_view{_lineBuffer.data(), _lineLength})) {
		return;
	}

	std::memcpy(_history[_historyWriteIndex].data(), _lineBuffer.data(), _lineLength);
	_history[_historyWriteIndex][_lineLength] = '\0';
	_historyLengths[_historyWriteIndex] = _lineLength;
	_historyWriteIndex = (_historyWriteIndex + 1) % maxHistoryEntries;
	if (_historySize < maxHistoryEntries) {
		++_historySize;
	}
	_markShellConfigDirty();
}

void Shell::_clearHistory() {
	for (auto& length : _historyLengths) {
		length = 0;
	}
	_historyWriteIndex = 0;
	_historySize = 0;
	_historyBrowseOffset = 0;
	_historyBrowsing = false;
}

void Shell::_storeHistoryEntry(std::string_view text) {
	if (text.empty() || _matchesLastHistoryEntry(text) || !_historyLineAllowed(text)) {
		return;
	}

	auto const length = text.size() < maxLineLength ? text.size() : maxLineLength - 1U;
	std::memcpy(_history[_historyWriteIndex].data(), text.data(), length);
	_history[_historyWriteIndex][length] = '\0';
	_historyLengths[_historyWriteIndex] = length;
	_historyWriteIndex = (_historyWriteIndex + 1) % maxHistoryEntries;
	if (_historySize < maxHistoryEntries) {
		++_historySize;
	}
}

void Shell::_loadHistoryEntry(std::size_t offset) {
	if (offset == 0 || offset > _historySize) {
		write("\a");
		return;
	}

	auto const index = (_historyWriteIndex + maxHistoryEntries - offset) % maxHistoryEntries;
	_replaceInputLine(_history[index].data(), _historyLengths[index]);
}

void Shell::_showPreviousHistoryEntry() {
	if (_historySize == 0) {
		write("\a");
		return;
	}

	if (!_historyBrowsing) {
		_saveHistoryDraft();
		_historyBrowsing = true;
		_historyBrowseOffset = 0;
	}

	if (_historyBrowseOffset >= _historySize) {
		write("\a");
		return;
	}

	++_historyBrowseOffset;
	_loadHistoryEntry(_historyBrowseOffset);
}

void Shell::_showNextHistoryEntry() {
	if (!_historyBrowsing) {
		return;
	}

	if (_historyBrowseOffset > 1) {
		--_historyBrowseOffset;
		_loadHistoryEntry(_historyBrowseOffset);
		return;
	}

	_replaceInputLine(_historyDraft.data(), _historyDraftLength);
	_historyBrowsing = false;
	_historyBrowseOffset = 0;
}

void Shell::_replaceInputLine(char const* text, std::size_t length) {
	if (length >= _lineBuffer.size()) {
		length = _lineBuffer.size() - 1;
	}

	std::memcpy(_lineBuffer.data(), text, length);
	_lineLength = length;
	_cursorPosition = _lineLength;
	_lineBuffer[_lineLength] = '\0';
	_redrawInputLine();
}

void Shell::_saveHistoryDraft() {
	std::memcpy(_historyDraft.data(), _lineBuffer.data(), _lineLength);
	_historyDraft[_lineLength] = '\0';
	_historyDraftLength = _lineLength;
}

void Shell::_stopHistoryBrowsing() {
	_historyBrowsing = false;
	_historyBrowseOffset = 0;
}

bool Shell::_acceptAutosuggestion() {
	auto const suggestion = _autosuggestionSuffix();
	if (suggestion.empty()) {
		return false;
	}
	_insertText(suggestion);
	return true;
}

bool Shell::_shouldRedrawForAutosuggestion() const {
	return _autosuggestEnabled && (_autosuggestionVisible || !_autosuggestionSuffix().empty());
}

std::string_view Shell::_autosuggestionSuffix() const {
	if (!_autosuggestEnabled || _historyBrowsing || _cursorPosition != _lineLength || _lineLength == 0) {
		return {};
	}

	for (std::size_t offset = 1; offset <= _historySize; ++offset) {
		auto const index = (_historyWriteIndex + maxHistoryEntries - offset) % maxHistoryEntries;
		auto const length = _historyLengths[index];
		if (length <= _lineLength) {
			continue;
		}
		if (std::memcmp(_history[index].data(), _lineBuffer.data(), _lineLength) == 0) {
			return std::string_view{_history[index].data() + _lineLength, length - _lineLength};
		}
	}
	return {};
}

bool Shell::_lineIsBlank() const {
	for (std::size_t index = 0; index < _lineLength; ++index) {
		if (!isSpace(_lineBuffer[index])) {
			return false;
		}
	}
	return true;
}

bool Shell::_matchesLastHistoryEntry(std::string_view text) const {
	if (_historySize == 0) {
		return false;
	}

	auto const index = (_historyWriteIndex + maxHistoryEntries - 1) % maxHistoryEntries;
	return _historyLengths[index] == text.size() && std::memcmp(_history[index].data(), text.data(), text.size()) == 0;
}

bool Shell::_matchesLastHistoryEntry() const {
	return _matchesLastHistoryEntry(std::string_view{_lineBuffer.data(), _lineLength});
}

void Shell::_handleInputKey(detail::ShellInputKey key) {
	switch (key) {
		case detail::ShellInputKey::ArrowUp:
			_showPreviousHistoryEntry();
			return;
		case detail::ShellInputKey::ArrowDown:
			_showNextHistoryEntry();
			return;
		case detail::ShellInputKey::ArrowRight:
			_moveCursorRight();
			return;
		case detail::ShellInputKey::ArrowLeft:
			_moveCursorLeft();
			return;
		case detail::ShellInputKey::Home:
			_moveCursorHome();
			return;
		case detail::ShellInputKey::End:
			_moveCursorEnd();
			return;
		case detail::ShellInputKey::Delete:
			_eraseCurrentChar();
			return;
	}
}

void Shell::_redrawInputLine() {
	write("\r");
	_writePrompt();
	write(std::string_view(_lineBuffer.data(), _lineLength));
	auto const suggestion = _autosuggestionSuffix();
	if (!suggestion.empty()) {
		write("\x1B[90m");
		write(suggestion);
		write("\x1B[0m");
		_autosuggestionVisible = true;
	} else {
		_autosuggestionVisible = false;
	}
	write("\x1B[K");
	_moveTerminalCursorLeft((_lineLength - _cursorPosition) + suggestion.size());
}

void Shell::_moveCursorLeft() {
	if (_cursorPosition == 0) {
		return;
	}
	--_cursorPosition;
	if (_autosuggestionVisible) {
		_redrawInputLine();
		return;
	}
	write("\x1B[D");
}

void Shell::_moveCursorRight() {
	if (_cursorPosition >= _lineLength) {
		if (_acceptAutosuggestion()) {
			return;
		}
		return;
	}
	++_cursorPosition;
	if (_shouldRedrawForAutosuggestion()) {
		_redrawInputLine();
		return;
	}
	write("\x1B[C");
}

void Shell::_moveCursorHome() {
	auto const offset = _cursorPosition;
	_cursorPosition = 0;
	if (_autosuggestionVisible) {
		_redrawInputLine();
		return;
	}
	_moveTerminalCursorLeft(offset);
}

void Shell::_moveCursorEnd() {
	if (_acceptAutosuggestion()) {
		return;
	}
	if (_cursorPosition >= _lineLength) {
		return;
	}
	auto const start = _cursorPosition;
	_cursorPosition = _lineLength;
	if (_shouldRedrawForAutosuggestion()) {
		_redrawInputLine();
		return;
	}
	write(std::string_view(_lineBuffer.data() + start, _lineLength - start));
}

void Shell::_moveTerminalCursorLeft(std::size_t count) {
	for (std::size_t index = 0; index < count; ++index) {
		write("\x1B[D");
	}
}

} // namespace lexxos::shell
