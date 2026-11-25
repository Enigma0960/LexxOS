#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::tui {

inline constexpr int glyphWidth = 5;
inline constexpr int glyphHeight = 7;
inline constexpr int cellWidth = 6;
inline constexpr int cellHeight = 8;
inline constexpr int maxLinePixels = 480;

enum class Action : std::uint8_t {
	None,
	Up,
	Down,
	Left,
	Right,
	Confirm,
	Back,
	Start,
	Select,
};

struct TextStyle {
	std::uint16_t foreground{0xFFFF};
	std::uint16_t background{0x0000};
};

struct Palette {
	std::uint16_t background{0x0841};
	std::uint16_t foreground{0xFFFF};
	std::uint16_t muted{0xBDF7};
	std::uint16_t accent{0x07FF};
	std::uint16_t selectedForeground{0x0000};
	std::uint16_t selectedBackground{0xFFE0};
	std::uint16_t danger{0xF800};
};

struct MenuState {
	std::size_t selected{0};
	std::size_t scroll{0};

	void clamp(std::size_t itemCount, std::size_t visibleRows);
	[[nodiscard]] bool moveUp(std::size_t itemCount, std::size_t visibleRows);
	[[nodiscard]] bool moveDown(std::size_t itemCount, std::size_t visibleRows);
};

} // namespace lexxos::tui
