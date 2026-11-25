#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/tui/tui_types.h>

namespace lexxos::devices {

class DisplayDevice;

} // namespace lexxos::devices

namespace lexxos::tui {

class Screen final {
public:
	explicit Screen(lexxos::devices::DisplayDevice& display);

	[[nodiscard]] int width() const;
	[[nodiscard]] int height() const;
	[[nodiscard]] int columns() const;
	[[nodiscard]] int rows() const;

	bool beginFrame();
	bool endFrame();
	bool clear(std::uint16_t color);
	bool drawText(int column, int row, std::string_view text, TextStyle style, bool fillToEnd = false);
	bool drawMenuItem(int row, std::string_view text, bool selected, Palette const& palette);

private:
	lexxos::devices::DisplayDevice& _display;
	std::array<std::uint16_t, maxLinePixels> _line{};

	[[nodiscard]] bool _drawTextPixels(int x, int y, int cells, std::string_view text, TextStyle style);
	static void _glyphRows(char character, std::uint8_t (&rows)[glyphHeight]);
};

} // namespace lexxos::tui
