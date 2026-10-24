#include <lexxos/apps/editor/detail/text_editor_types.h>

#include <cstring>

namespace lexxos::apps::text_editor_programs::detail {

void TextBuffer::clear() {
	lineCount = 0;
	modified = false;
	for (auto& length : lengths) {
		length = 0;
	}
	for (auto& lineBuffer : lines) {
		lineBuffer[0] = '\0';
	}
}

bool TextBuffer::full() const {
	return lineCount >= maxEditorLines;
}

std::string_view TextBuffer::line(std::size_t index) const {
	if (index >= lineCount) {
		return {};
	}
	return {lines[index].data(), lengths[index]};
}

std::size_t TextBuffer::lineLength(std::size_t index) const {
	return index < lineCount ? lengths[index] : 0;
}

bool TextBuffer::setLine(std::size_t index, std::string_view text) {
	if (index >= lineCount || text.size() >= maxEditorLineLength) {
		return false;
	}
	if (!text.empty()) {
		std::memcpy(lines[index].data(), text.data(), text.size());
	}
	lines[index][text.size()] = '\0';
	lengths[index] = static_cast<std::uint8_t>(text.size());
	modified = true;
	return true;
}

bool TextBuffer::insertLine(std::size_t index, std::string_view text) {
	if (index > lineCount || full() || text.size() >= maxEditorLineLength) {
		return false;
	}
	for (std::size_t cursor = lineCount; cursor > index; --cursor) {
		lines[cursor] = lines[cursor - 1];
		lengths[cursor] = lengths[cursor - 1];
	}
	++lineCount;
	if (!setLine(index, text)) {
		return false;
	}
	modified = true;
	return true;
}

bool TextBuffer::appendLine(std::string_view text) {
	return insertLine(lineCount, text);
}

bool TextBuffer::deleteLine(std::size_t index) {
	if (index >= lineCount) {
		return false;
	}
	for (std::size_t cursor = index; cursor + 1 < lineCount; ++cursor) {
		lines[cursor] = lines[cursor + 1];
		lengths[cursor] = lengths[cursor + 1];
	}
	--lineCount;
	lengths[lineCount] = 0;
	lines[lineCount][0] = '\0';
	modified = true;
	return true;
}

bool TextBuffer::ensureEditableLine() {
	if (lineCount != 0) {
		return true;
	}
	return appendLine({});
}

bool TextBuffer::insertChar(std::size_t lineIndex, std::size_t column, char value) {
	if (!ensureEditableLine() || lineIndex >= lineCount) {
		return false;
	}
	auto const length = lengths[lineIndex];
	if (column > length || length + 1U >= maxEditorLineLength) {
		return false;
	}
	for (std::size_t cursor = length; cursor > column; --cursor) {
		lines[lineIndex][cursor] = lines[lineIndex][cursor - 1];
	}
	lines[lineIndex][column] = value;
	lengths[lineIndex] = static_cast<std::uint8_t>(length + 1U);
	lines[lineIndex][length + 1U] = '\0';
	modified = true;
	return true;
}

bool TextBuffer::deleteChar(std::size_t lineIndex, std::size_t column) {
	if (lineIndex >= lineCount || column >= lengths[lineIndex]) {
		return false;
	}
	auto const length = lengths[lineIndex];
	for (std::size_t cursor = column; cursor + 1U < length; ++cursor) {
		lines[lineIndex][cursor] = lines[lineIndex][cursor + 1U];
	}
	lengths[lineIndex] = static_cast<std::uint8_t>(length - 1U);
	lines[lineIndex][length - 1U] = '\0';
	modified = true;
	return true;
}

bool TextBuffer::splitLine(std::size_t lineIndex, std::size_t column) {
	if (!ensureEditableLine() || lineIndex >= lineCount || column > lengths[lineIndex] || full()) {
		return false;
	}

	auto const length = lengths[lineIndex];
	auto const tailLength = length - column;
	std::array<char, maxEditorLineLength> tail{};
	if (tailLength != 0) {
		std::memcpy(tail.data(), lines[lineIndex].data() + column, tailLength);
	}
	tail[tailLength] = '\0';

	lengths[lineIndex] = static_cast<std::uint8_t>(column);
	lines[lineIndex][column] = '\0';
	return insertLine(lineIndex + 1U, {tail.data(), tailLength});
}

bool TextBuffer::joinWithNextLine(std::size_t lineIndex) {
	if (lineIndex + 1U >= lineCount) {
		return false;
	}
	auto const leftLength = lengths[lineIndex];
	auto const rightLength = lengths[lineIndex + 1U];
	if (leftLength + rightLength >= maxEditorLineLength) {
		return false;
	}
	if (rightLength != 0) {
		std::memcpy(lines[lineIndex].data() + leftLength, lines[lineIndex + 1U].data(), rightLength);
	}
	lengths[lineIndex] = static_cast<std::uint8_t>(leftLength + rightLength);
	lines[lineIndex][leftLength + rightLength] = '\0';
	(void) deleteLine(lineIndex + 1U);
	modified = true;
	return true;
}

} // namespace lexxos::apps::text_editor_programs::detail
