#include <lexxos/shell/shell.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

#include <lexxos/os/operating_system.h>
#include <lexxos/vfs/vfs.h>

namespace {

// Config loading can run inside the console command stack. Keep both large
// parsing buffers in one temporary allocation, released on every return path.
struct ShellConfigWorkspace {
	std::array<char, lexxos::shell::Shell::maxLineLength + 64U> line{};
	std::array<char, lexxos::shell::Shell::maxLineLength> decoded{};
};

constexpr std::string_view shellConfigRoot = "/mnt/sd";
constexpr std::string_view shellConfigDirectory = "/mnt/sd/.lexx";
constexpr std::string_view historyPrefix = "history=";
constexpr std::string_view autosuggestPrefix = "autosuggest=";

bool startsWith(std::string_view text, std::string_view prefix) {
	return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string_view trim(std::string_view text) {
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
		text.remove_suffix(1);
	}
	return text;
}

bool parseBool(std::string_view text, bool& output) {
	text = trim(text);
	if (text == "1" || text == "on" || text == "yes" || text == "true") {
		output = true;
		return true;
	}
	if (text == "0" || text == "off" || text == "no" || text == "false") {
		output = false;
		return true;
	}
	return false;
}

char hexDigit(std::uint8_t value) {
	return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
}

int hexValue(char value) {
	if (value >= '0' && value <= '9') {
		return value - '0';
	}
	if (value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	}
	if (value >= 'A' && value <= 'F') {
		return value - 'A' + 10;
	}
	return -1;
}

bool writeAll(lexxos::vfs::FileHandle& file, std::string_view text) {
	return file.write(text) == lexxos::vfs::WriteStatus::Ok;
}

bool writeEscaped(lexxos::vfs::FileHandle& file, std::string_view text) {
	for (char const character : text) {
		auto const value = static_cast<std::uint8_t>(character);
		if (character == '\\') {
			if (!writeAll(file, "\\\\")) {
				return false;
			}
			continue;
		}
		if (character == '\t') {
			if (!writeAll(file, "\\t")) {
				return false;
			}
			continue;
		}
		if (value < 0x20U || value > 0x7EU) {
			char escape[4]{'\\', 'x', hexDigit(static_cast<std::uint8_t>((value >> 4U) & 0x0FU)), hexDigit(static_cast<std::uint8_t>(value & 0x0FU))};
			if (!writeAll(file, std::string_view{escape, sizeof(escape)})) {
				return false;
			}
			continue;
		}
		if (!writeAll(file, std::string_view{&character, 1})) {
			return false;
		}
	}
	return true;
}

bool decodeEscaped(
    std::string_view text,
    std::array<char, lexxos::shell::Shell::maxLineLength>& output,
    std::size_t& outputLength) {
	outputLength = 0;
	for (std::size_t index = 0; index < text.size(); ++index) {
		char value = text[index];
		if (value == '\\') {
			if (index + 1 >= text.size()) {
				return false;
			}
			value = text[++index];
			if (value == 't') {
				value = '\t';
			} else if (value == 'n') {
				value = '\n';
			} else if (value == 'r') {
				value = '\r';
			} else if (value == 'x') {
				if (index + 2 >= text.size()) {
					return false;
				}
				auto const high = hexValue(text[++index]);
				auto const low = hexValue(text[++index]);
				if (high < 0 || low < 0) {
					return false;
				}
				value = static_cast<char>((high << 4) | low);
			}
		}
		if (outputLength + 1 >= output.size()) {
			return false;
		}
		output[outputLength++] = value;
	}
	output[outputLength] = '\0';
	return true;
}

enum class LineReadStatus : std::uint8_t {
	Line,
	End,
	Error,
};

LineReadStatus readConfigLine(
    lexxos::vfs::FileHandle& file,
    std::array<char, lexxos::shell::Shell::maxLineLength + 64U>& line,
    std::size_t& lineLength) {
	lineLength = 0;
	bool overflow = false;
	for (;;) {
		char character{};
		std::size_t bytesRead = 0;
		auto const status = file.read(&character, 1, bytesRead);
		if (status != lexxos::vfs::ReadStatus::Ok) {
			return LineReadStatus::Error;
		}
		if (bytesRead == 0) {
			if (lineLength == 0 && !overflow) {
				return LineReadStatus::End;
			}
			line[lineLength] = '\0';
			return LineReadStatus::Line;
		}
		if (character == '\n') {
			line[lineLength] = '\0';
			return LineReadStatus::Line;
		}
		if (character == '\r') {
			continue;
		}
		if (lineLength + 1 < line.size()) {
			line[lineLength++] = character;
		} else {
			overflow = true;
		}
	}
}

} // namespace

namespace lexxos::shell {

bool Shell::autosuggestEnabled() const {
	return _autosuggestEnabled;
}

std::size_t Shell::historySize() const {
	return _historySize;
}

bool Shell::shellConfigLoaded() const {
	return _shellConfigLoaded;
}

void Shell::setAutosuggestEnabled(bool enabled) {
	if (_autosuggestEnabled == enabled) {
		return;
	}
	_autosuggestEnabled = enabled;
	_autosuggestionVisible = false;
	_markShellConfigDirty();
}

bool Shell::loadShellConfig(bool force) {
	if (!force && _shellConfigLoaded) {
		return true;
	}
	if (!_shellConfigStorageReady()) {
		return false;
	}
	return _readShellConfig(force || _historySize == 0);
}

bool Shell::saveShellConfig() {
	if (!_writeShellConfig()) {
		return false;
	}
	_shellConfigDirty = false;
	_shellConfigLoaded = true;
	return true;
}

lexxos::vfs::Vfs* Shell::_vfs() const {
	return _operatingSystem != nullptr ? &_operatingSystem->vfs() : nullptr;
}

bool Shell::_shellConfigStorageReady() const {
	auto* vfs = _vfs();
	if (vfs == nullptr) {
		return false;
	}

	lexxos::vfs::Stat node{};
	return vfs->stat(shellConfigRoot, node) && node.type == lexxos::vfs::NodeType::Directory;
}

bool Shell::_ensureShellConfigDirectory() {
	auto* vfs = _vfs();
	if (vfs == nullptr || !_shellConfigStorageReady()) {
		return false;
	}

	lexxos::vfs::Stat node{};
	if (vfs->stat(shellConfigDirectory, node)) {
		return node.type == lexxos::vfs::NodeType::Directory;
	}
	auto const status = vfs->createDirectory(shellConfigDirectory);
	return status == lexxos::vfs::WriteStatus::Ok || status == lexxos::vfs::WriteStatus::IsDirectory;
}

void Shell::_loadShellConfigIfAvailable() {
	if (_shellConfigLoaded) {
		return;
	}
	(void) loadShellConfig(false);
}

void Shell::_markShellConfigDirty() {
	_shellConfigDirty = true;
	_trySaveShellConfigIfDirty();
}

void Shell::_trySaveShellConfigIfDirty() {
	if (!_shellConfigDirty) {
		return;
	}
	if (!_shellConfigLoaded && !loadShellConfig(false)) {
		return;
	}
	(void) saveShellConfig();
}

bool Shell::_writeShellConfig() {
	if (!_ensureShellConfigDirectory()) {
		return false;
	}

	auto* vfs = _vfs();
	if (vfs == nullptr) {
		return false;
	}

	lexxos::vfs::FileHandle file;
	if (vfs->open(configPath, lexxos::vfs::OpenMode::CreateTruncateWrite, file) != lexxos::vfs::OpenStatus::Ok) {
		return false;
	}

	bool ok = writeAll(file, "# LexxOS shell settings\n")
	    && writeAll(file, "autosuggest=")
	    && writeAll(file, _autosuggestEnabled ? "on\n" : "off\n");

	for (std::size_t offset = _historySize; ok && offset > 0; --offset) {
		auto const index = (_historyWriteIndex + maxHistoryEntries - offset) % maxHistoryEntries;
		if (!_historyLineAllowed(std::string_view{_history[index].data(), _historyLengths[index]})) {
			continue;
		}
		ok = writeAll(file, historyPrefix)
		    && writeEscaped(file, std::string_view{_history[index].data(), _historyLengths[index]})
		    && writeAll(file, "\n");
	}

	file.close();
	return ok && vfs->sync() == lexxos::vfs::SyncStatus::Ok;
}

bool Shell::_readShellConfig(bool loadHistory) {
	auto* vfs = _vfs();
	if (vfs == nullptr) {
		return false;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = vfs->open(configPath, lexxos::vfs::OpenMode::Read, file);
	if (openStatus == lexxos::vfs::OpenStatus::NotFound) {
		_shellConfigLoaded = true;
		return true;
	}
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		return false;
	}

	auto workspace = std::unique_ptr<ShellConfigWorkspace>{new (std::nothrow) ShellConfigWorkspace{}};
	if (!workspace) {
		return false;
	}

	if (loadHistory) {
		_clearHistory();
	}

	auto& line = workspace->line;
	for (;;) {
		std::size_t lineLength = 0;
		auto const status = readConfigLine(file, line, lineLength);
		if (status == LineReadStatus::End) {
			break;
		}
		if (status == LineReadStatus::Error) {
			return false;
		}
		(void) _parseShellConfigLine(std::string_view{line.data(), lineLength}, loadHistory, workspace->decoded);
	}

	_shellConfigLoaded = true;
	_shellConfigDirty = false;
	return true;
}

bool Shell::_parseShellConfigLine(
    std::string_view line, bool loadHistory, std::array<char, maxLineLength>& decoded) {
	line = trim(line);
	if (line.empty() || line.front() == '#') {
		return true;
	}

	if (startsWith(line, autosuggestPrefix)) {
		bool enabled = _autosuggestEnabled;
		if (!parseBool(line.substr(autosuggestPrefix.size()), enabled)) {
			return false;
		}
		_autosuggestEnabled = enabled;
		return true;
	}

	if (startsWith(line, historyPrefix)) {
		if (!loadHistory) {
			return true;
		}
		std::size_t decodedLength = 0;
		if (!decodeEscaped(line.substr(historyPrefix.size()), decoded, decodedLength)) {
			return false;
		}
		_storeHistoryEntry(std::string_view{decoded.data(), decodedLength});
		return true;
	}

	return true;
}

} // namespace lexxos::shell
