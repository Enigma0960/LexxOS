#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/program_input.h>
#include <lexxos/devices/gamepad_device.h>

namespace lexxos::devices {
class DisplayDevice;
}

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

class InputReader final {
public:
	explicit InputReader(lexxos::devices::GamepadDevice* gamepad);

	void reset();
	[[nodiscard]] bool poll(Action& action);

private:
	lexxos::devices::GamepadDevice* _gamepad{nullptr};
	lexxos::devices::GamepadState _previousGamepad{};
	lexxos::app::program_input::EventCursor _inputCursor{};
	bool _ready{false};

	[[nodiscard]] lexxos::devices::GamepadState _gamepadState() const;
	[[nodiscard]] static Action _actionForInputEvent(lexxos::devices::InputEvent const& event);
	[[nodiscard]] static Action _actionForGamepadEdge(
	    lexxos::devices::GamepadState const& previous,
	    lexxos::devices::GamepadState const& current);
};

struct MenuState {
	std::size_t selected{0};
	std::size_t scroll{0};

	void clamp(std::size_t itemCount, std::size_t visibleRows);
	[[nodiscard]] bool moveUp(std::size_t itemCount, std::size_t visibleRows);
	[[nodiscard]] bool moveDown(std::size_t itemCount, std::size_t visibleRows);
};

} // namespace lexxos::tui
