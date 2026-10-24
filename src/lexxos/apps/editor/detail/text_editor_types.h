#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::apps::text_editor_programs::detail {

inline constexpr std::size_t maxEditorLines = 64;
inline constexpr std::size_t maxEditorLineLength = 96;

struct TextBuffer {
	std::array<std::array<char, maxEditorLineLength>, maxEditorLines> lines{};
	std::array<std::uint8_t, maxEditorLines> lengths{};
	std::size_t lineCount{0};
	bool modified{false};

	void clear();
	[[nodiscard]] bool full() const;
	[[nodiscard]] std::string_view line(std::size_t index) const;
	[[nodiscard]] std::size_t lineLength(std::size_t index) const;
	[[nodiscard]] bool setLine(std::size_t index, std::string_view text);
	[[nodiscard]] bool insertLine(std::size_t index, std::string_view text);
	[[nodiscard]] bool appendLine(std::string_view text);
	[[nodiscard]] bool deleteLine(std::size_t index);
	[[nodiscard]] bool ensureEditableLine();
	[[nodiscard]] bool insertChar(std::size_t lineIndex, std::size_t column, char value);
	[[nodiscard]] bool deleteChar(std::size_t lineIndex, std::size_t column);
	[[nodiscard]] bool splitLine(std::size_t lineIndex, std::size_t column);
	[[nodiscard]] bool joinWithNextLine(std::size_t lineIndex);
};

struct EditorState {
	std::size_t cursorLine{0};
	std::size_t cursorColumn{0};
	std::size_t topLine{0};
	std::size_t leftColumn{0};
	std::array<char, 80> status{};
	bool confirmQuit{false};
};

struct EditorSession {
	TextBuffer buffer{};
	EditorState state{};
};

} // namespace lexxos::apps::text_editor_programs::detail
