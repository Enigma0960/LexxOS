#include <lexxos/shell/shell.h>
#include <lexxos/shell/shell_completion_types.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

#include <lexxos/os/operating_system.h>
#include <lexxos/vfs/vfs.h>

namespace {

bool isSpace(char value) {
	return value == ' ' || value == '\t';
}

bool startsWith(std::string_view value, std::string_view prefix) {
	return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool looksLikePathToken(std::string_view token) {
	return token.empty()
	    || token.front() == '/'
	    || token.front() == '.'
	    || token.find('/') != std::string_view::npos;
}

std::size_t commonPrefixLength(std::string_view left, std::string_view right) {
	std::size_t length = 0;
	while (length < left.size() && length < right.size() && left[length] == right[length]) {
		++length;
	}
	return length;
}

} // namespace

namespace lexxos::shell {

void Shell::_completeInputLine() {
	std::size_t tokenStart = 0;
	std::size_t tokenEnd = 0;
	bool programToken = false;

	if (!_findCompletionToken(tokenStart, tokenEnd, programToken)) {
		write("\a");
		return;
	}

	if (programToken) {
		if (tokenStart == tokenEnd) {
			write("\a");
			return;
		}
		_completeProgramName(tokenStart, tokenEnd);
		return;
	}

	_completePath(tokenStart, tokenEnd);
}

void Shell::_completeProgramName(std::size_t tokenStart, std::size_t tokenEnd) {
	auto const prefix = std::string_view(_lineBuffer.data() + tokenStart, tokenEnd - tokenStart);
	if (_operatingSystem == nullptr) {
		write("\a");
		return;
	}
	auto const& programs = _operatingSystem->programs();

	std::array<char, maxLineLength> commonPrefix{};
	std::size_t commonLength = 0;
	std::size_t matchCount = 0;

	for (std::size_t index = 0; index < programs.size(); ++index) {
		auto const& descriptor = programs.at(index);
		if (!descriptor.enabled || !startsWith(descriptor.name, prefix)) {
			continue;
		}

		if (matchCount == 0) {
			commonLength = descriptor.name.size();
			if (commonLength >= commonPrefix.size()) {
				commonLength = commonPrefix.size() - 1;
			}
			std::memcpy(commonPrefix.data(), descriptor.name.data(), commonLength);
			commonPrefix[commonLength] = '\0';
		} else {
			commonLength = commonPrefixLength(std::string_view(commonPrefix.data(), commonLength), descriptor.name);
			commonPrefix[commonLength] = '\0';
		}

		++matchCount;
	}

	if (matchCount == 0) {
		write("\a");
		return;
	}

	if (matchCount == 1) {
		_replaceInputRange(tokenStart, tokenEnd, std::string_view(commonPrefix.data(), commonLength));
		if (_cursorPosition >= _lineLength || !isSpace(_lineBuffer[_cursorPosition])) {
			_insertText(" ");
		}
		return;
	}

	if (commonLength > prefix.size()) {
		_replaceInputRange(tokenStart, tokenEnd, std::string_view(commonPrefix.data(), commonLength));
		return;
	}

	writeLine("");
	_writeProgramCompletionMatches(prefix);
	_redrawInputLine();
}

void Shell::_completePath(std::size_t tokenStart, std::size_t tokenEnd) {
	auto const token = std::string_view(_lineBuffer.data() + tokenStart, tokenEnd - tokenStart);

	std::string_view typedDirectoryPrefix{};
	std::string_view entryPrefix = token;
	auto const slash = token.rfind('/');
	if (slash != std::string_view::npos) {
		typedDirectoryPrefix = token.substr(0, slash + 1);
		entryPrefix = token.substr(slash + 1);
	}

	std::array<char, lexxos::vfs::maxPathLength> directoryPath{};
	auto const directoryToken = typedDirectoryPrefix.empty() ? std::string_view{"."} : typedDirectoryPrefix;
	if (!lexxos::vfs::normalizePath(workingDirectory(), directoryToken, directoryPath.data(), directoryPath.size())) {
		write("\a");
		return;
	}

	detail::PathCompletionContext context{.prefix = entryPrefix};
	auto const visitor = [](lexxos::vfs::DirectoryEntry const& entry, void* userData) -> bool {
		auto& target = *static_cast<detail::PathCompletionContext*>(userData);
		if (!startsWith(entry.name, target.prefix)) {
			return true;
		}

		if (target.matchCount == 0) {
			target.commonLength = entry.name.size();
			if (target.commonLength >= target.commonName.size()) {
				target.commonLength = target.commonName.size() - 1;
			}
			std::memcpy(target.commonName.data(), entry.name.data(), target.commonLength);
			target.commonName[target.commonLength] = '\0';
			target.singleType = entry.type;
		} else {
			target.commonLength = commonPrefixLength(std::string_view(target.commonName.data(), target.commonLength), entry.name);
			target.commonName[target.commonLength] = '\0';
		}

		++target.matchCount;
		return true;
	};

	if (!lexxos::vfs::list(directoryPath.data(), visitor, &context)) {
		write("\a");
		return;
	}

	if (context.matchCount == 0) {
		write("\a");
		return;
	}

	std::array<char, maxLineLength> replacement{};
	std::size_t replacementLength = 0;
	auto appendReplacementText = [&](std::string_view text) -> bool {
		if (replacementLength + text.size() >= replacement.size()) {
			return false;
		}
		std::memcpy(replacement.data() + replacementLength, text.data(), text.size());
		replacementLength += text.size();
		replacement[replacementLength] = '\0';
		return true;
	};

	if (!appendReplacementText(typedDirectoryPrefix)
	    || !appendReplacementText(std::string_view(context.commonName.data(), context.commonLength))) {
		write("\a");
		return;
	}

	if (context.matchCount == 1) {
		if (context.singleType == lexxos::vfs::NodeType::Directory) {
			if (!appendReplacementText("/")) {
				write("\a");
				return;
			}
		} else {
			if (!appendReplacementText(" ")) {
				write("\a");
				return;
			}
		}
		_replaceInputRange(tokenStart, tokenEnd, std::string_view(replacement.data(), replacementLength));
		return;
	}

	if (context.commonLength > entryPrefix.size()) {
		_replaceInputRange(tokenStart, tokenEnd, std::string_view(replacement.data(), replacementLength));
		return;
	}

	writeLine("");
	_writePathCompletionMatches(directoryPath.data(), entryPrefix);
	_redrawInputLine();
}

void Shell::_writeProgramCompletionMatches(std::string_view prefix) {
	if (_operatingSystem == nullptr) {
		return;
	}
	auto const& programs = _operatingSystem->programs();
	for (std::size_t index = 0; index < programs.size(); ++index) {
		auto const& descriptor = programs.at(index);
		if (!descriptor.enabled || !startsWith(descriptor.name, prefix)) {
			continue;
		}
		write("  ");
		writeLine(descriptor.name);
	}
}

void Shell::_writePathCompletionMatches(std::string_view directoryPath, std::string_view prefix) {
	detail::PathCompletionMatchWriterContext context{.shell = this, .prefix = prefix};
	lexxos::vfs::list(
	    directoryPath,
	    [](lexxos::vfs::DirectoryEntry const& entry, void* userData) -> bool {
		    auto& target = *static_cast<detail::PathCompletionMatchWriterContext*>(userData);
		    if (!startsWith(entry.name, target.prefix)) {
			    return true;
		    }

		    target.shell->write("  ");
		    target.shell->write(entry.name);
		    if (entry.type == lexxos::vfs::NodeType::Directory) {
			    target.shell->write("/");
		    }
		    target.shell->writeLine("");
		    return true;
	    },
	    &context);
}

bool Shell::_findCompletionToken(std::size_t& tokenStart, std::size_t& tokenEnd, bool& programToken) const {
	programToken = false;
	if (_cursorPosition > _lineLength) {
		return false;
	}

	tokenStart = _cursorPosition;
	while (tokenStart > 0 && !isSpace(_lineBuffer[tokenStart - 1])) {
		--tokenStart;
	}

	tokenEnd = _cursorPosition;
	while (tokenEnd < _lineLength && !isSpace(_lineBuffer[tokenEnd])) {
		++tokenEnd;
	}

	if (_cursorPosition != tokenEnd) {
		return false;
	}

	std::size_t firstTokenStart = 0;
	while (firstTokenStart < _lineLength && isSpace(_lineBuffer[firstTokenStart])) {
		++firstTokenStart;
	}

	if (firstTokenStart == _lineLength) {
		tokenStart = _cursorPosition;
		tokenEnd = _cursorPosition;
		programToken = false;
		return true;
	}

	if (tokenStart == firstTokenStart) {
		auto const token = std::string_view(_lineBuffer.data() + tokenStart, tokenEnd - tokenStart);
		programToken = !looksLikePathToken(token);
	} else {
		programToken = false;
	}
	return true;
}

} // namespace lexxos::shell
