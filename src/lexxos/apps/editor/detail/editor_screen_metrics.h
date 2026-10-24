#pragma once

#include <cstdint>

namespace lexxos::apps::text_editor_programs::detail {

inline constexpr std::uint8_t defaultEditorScreenRows = 24;
inline constexpr std::uint8_t defaultEditorScreenColumns = 80;
inline constexpr std::uint8_t editorLinePrefixColumns = 5;

struct EditorScreenMetrics {
	std::uint8_t screenRows{defaultEditorScreenRows};
	std::uint8_t screenColumns{defaultEditorScreenColumns};
	std::uint8_t headerRow{1};
	std::uint8_t firstTextRow{2};
	std::uint8_t statusRow{defaultEditorScreenRows};
	std::uint8_t visibleTextRows{defaultEditorScreenRows - 2U};
	std::uint8_t linePrefixColumns{editorLinePrefixColumns};
	std::uint8_t visibleTextColumns{defaultEditorScreenColumns - editorLinePrefixColumns};
};

[[nodiscard]] constexpr EditorScreenMetrics makeEditorScreenMetrics(std::uint8_t rows, std::uint8_t columns) {
	if (rows < 3U || columns <= editorLinePrefixColumns) {
		return {};
	}

	return {
	    .screenRows = rows,
	    .screenColumns = columns,
	    .headerRow = 1,
	    .firstTextRow = 2,
	    .statusRow = rows,
	    .visibleTextRows = static_cast<std::uint8_t>(rows - 2U),
	    .linePrefixColumns = editorLinePrefixColumns,
	    .visibleTextColumns = static_cast<std::uint8_t>(columns - editorLinePrefixColumns),
	};
}

[[nodiscard]] constexpr EditorScreenMetrics defaultEditorScreenMetrics() {
	return makeEditorScreenMetrics(defaultEditorScreenRows, defaultEditorScreenColumns);
}

} // namespace lexxos::apps::text_editor_programs::detail
