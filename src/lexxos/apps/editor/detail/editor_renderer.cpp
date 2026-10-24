#include <lexxos/apps/editor/detail/editor_renderer.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/editor/detail/editor_screen_metrics.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/terminal/terminal.h>

namespace lexxos::apps::text_editor_programs::detail {
namespace {

	void appendChar(char* buffer, std::size_t capacity, std::size_t& length, char value) {
		if (length + 1U >= capacity) {
			return;
		}
		buffer[length++] = value;
		buffer[length] = '\0';
	}

	void appendText(char* buffer, std::size_t capacity, std::size_t& length, std::string_view text) {
		for (char const value : text) {
			appendChar(buffer, capacity, length, value);
		}
	}

	void appendUnsigned(char* buffer, std::size_t capacity, std::size_t& length, std::uint32_t value) {
		char digits[10]{};
		std::size_t digitCount = 0;
		if (value == 0) {
			digits[digitCount++] = '0';
		} else {
			while (value != 0 && digitCount < sizeof(digits)) {
				digits[digitCount++] = static_cast<char>('0' + (value % 10U));
				value /= 10U;
			}
		}
		while (digitCount != 0) {
			appendChar(buffer, capacity, length, digits[--digitCount]);
		}
	}

	void appendPaddedUnsigned(char* buffer, std::size_t capacity, std::size_t& length,
	    std::uint32_t value, std::uint8_t width) {
		char digits[10]{};
		std::uint8_t digitCount = 0;
		std::uint32_t cursor = value;
		if (cursor == 0) {
			digits[digitCount++] = '0';
		} else {
			while (cursor != 0 && digitCount < sizeof(digits)) {
				digits[digitCount++] = static_cast<char>('0' + (cursor % 10U));
				cursor /= 10U;
			}
		}
		while (digitCount < width) {
			appendChar(buffer, capacity, length, ' ');
			--width;
		}
		while (digitCount != 0) {
			appendChar(buffer, capacity, length, digits[--digitCount]);
		}
	}

	void writeClipped(lexxos::io::OutputStream& output, std::string_view text, std::size_t maxLength) {
		auto const length = std::min<std::size_t>(text.size(), maxLength);
		if (length != 0) {
			output.write(text.substr(0, length));
		}
	}

	void clampCursorToBuffer(TextBuffer const& buffer, EditorState& state) {
		if (buffer.lineCount == 0) {
			state.cursorLine = 0;
			state.cursorColumn = 0;
			return;
		}
		if (state.cursorLine >= buffer.lineCount) {
			state.cursorLine = buffer.lineCount - 1U;
		}
		auto const length = buffer.lineLength(state.cursorLine);
		if (state.cursorColumn > length) {
			state.cursorColumn = length;
		}
	}

	void ensureCursorVisible(TextBuffer const& buffer, EditorState& state, EditorScreenMetrics const& metrics) {
		clampCursorToBuffer(buffer, state);

		if (state.cursorLine < state.topLine) {
			state.topLine = state.cursorLine;
		}
		if (state.cursorLine >= state.topLine + metrics.visibleTextRows) {
			state.topLine = state.cursorLine - metrics.visibleTextRows + 1U;
		}

		if (state.cursorColumn < state.leftColumn) {
			state.leftColumn = state.cursorColumn;
		}
		if (state.cursorColumn >= state.leftColumn + metrics.visibleTextColumns) {
			state.leftColumn = state.cursorColumn - metrics.visibleTextColumns + 1U;
		}
	}

	void renderHeader(lexxos::io::OutputStream& output, std::string_view path, TextBuffer const& buffer,
	    EditorScreenMetrics const& metrics) {
		lexxos::terminal::moveCursor(output, metrics.headerRow, 1);
		lexxos::terminal::eraseLine(output);
		output.write(" LexxOS edit ");
		output.write(buffer.modified ? "* " : "  ");
		writeClipped(output, path, metrics.screenColumns > 16U ? metrics.screenColumns - 16U : 0U);
	}

	void renderTextRows(lexxos::io::OutputStream& output, TextBuffer const& buffer, EditorState const& state,
	    EditorScreenMetrics const& metrics) {
		for (std::uint8_t row = 0; row < metrics.visibleTextRows; ++row) {
			auto const bufferLine = state.topLine + row;
			lexxos::terminal::moveCursor(output, static_cast<std::uint8_t>(metrics.firstTextRow + row), 1);
			lexxos::terminal::eraseLine(output);

			if (bufferLine >= buffer.lineCount) {
				if (bufferLine == 0 && buffer.lineCount == 0) {
					output.write("   1 ");
				} else {
					output.write("~");
				}
				continue;
			}

			char prefix[8]{};
			std::size_t prefixLength = 0;
			appendPaddedUnsigned(prefix, sizeof(prefix), prefixLength,
			    static_cast<std::uint32_t>(bufferLine + 1U), 4);
			appendChar(prefix, sizeof(prefix), prefixLength, ' ');
			output.write({prefix, prefixLength});

			auto const line = buffer.line(bufferLine);
			if (state.leftColumn >= line.size()) {
				continue;
			}
			writeClipped(output, line.substr(state.leftColumn), metrics.visibleTextColumns);
		}
	}

	void renderStatus(lexxos::io::OutputStream& output, TextBuffer const& buffer, EditorState const& state,
	    EditorScreenMetrics const& metrics) {
		lexxos::terminal::moveCursor(output, metrics.statusRow, 1);
		lexxos::terminal::eraseLine(output);

		if (state.status[0] != '\0') {
			writeClipped(output, state.status.data(), metrics.screenColumns);
			return;
		}

		char line[maxEditorLineLength]{};
		std::size_t length = 0;
		appendText(line, sizeof(line), length, "^S save  ^Q quit  arrows move  Enter new line  ");
		appendUnsigned(line, sizeof(line), length, static_cast<std::uint32_t>(buffer.lineCount));
		appendText(line, sizeof(line), length, buffer.lineCount == 1 ? " line" : " lines");
		if (buffer.modified) {
			appendText(line, sizeof(line), length, "  modified");
		}
		writeClipped(output, {line, length}, metrics.screenColumns);
	}

} // namespace

void EditorRenderer::render(lexxos::io::OutputStream& output, std::string_view path,
    TextBuffer const& buffer, EditorState& state, EditorScreenMetrics const& metrics) {
	ensureCursorVisible(buffer, state, metrics);
	lexxos::terminal::hideCursor(output);
	renderHeader(output, path, buffer, metrics);
	renderTextRows(output, buffer, state, metrics);
	renderStatus(output, buffer, state, metrics);

	auto const cursorRow = static_cast<std::uint8_t>(metrics.firstTextRow + (state.cursorLine - state.topLine));
	auto const cursorColumn = static_cast<std::uint8_t>(metrics.linePrefixColumns + 1U + (state.cursorColumn - state.leftColumn));
	lexxos::terminal::moveCursor(output, cursorRow, cursorColumn);
	lexxos::terminal::showCursor(output);
}

void EditorRenderer::writeExitLine(lexxos::io::OutputStream& output) {
	lexxos::terminal::showCursor(output);
	lexxos::terminal::clearScreen(output);
	lexxos::terminal::moveCursor(output, 1, 1);
}

} // namespace lexxos::apps::text_editor_programs::detail
