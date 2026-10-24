#include <lexxos/tui/tui.h>

#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::tui {
namespace {

	bool pressed(bool previous, bool current) {
		return current && !previous;
	}

	void fillLine(std::uint16_t* pixels, int count, std::uint16_t color) {
		for (int index = 0; index < count; ++index) {
			pixels[index] = color;
		}
	}

} // namespace

Screen::Screen(lexxos::devices::DisplayDevice& display)
    : _display(display) {
}

int Screen::width() const {
	return _display.width();
}

int Screen::height() const {
	return _display.height();
}

int Screen::columns() const {
	return width() / cellWidth;
}

int Screen::rows() const {
	return height() / cellHeight;
}

bool Screen::beginFrame() {
	return _display.beginFrame();
}

bool Screen::endFrame() {
	return _display.endFrame();
}

bool Screen::clear(std::uint16_t color) {
	auto const full = lexxos::devices::Rect{.x = 0, .y = 0, .width = width(), .height = height()};
	if (_display.fillRgb565(full, color)) {
		return true;
	}

	if (width() <= 0 || width() > maxLinePixels) {
		return false;
	}
	fillLine(_line.data(), width(), color);
	for (int y = 0; y < height(); ++y) {
		if (!_display.blitRgb565({.x = 0, .y = y, .width = width(), .height = 1}, _line.data(), width())) {
			return false;
		}
	}
	return true;
}

bool Screen::drawText(int column, int row, std::string_view text, TextStyle style, bool fillToEnd) {
	if (column < 0 || row < 0 || column >= columns() || row >= rows()) {
		return false;
	}

	auto const availableCells = columns() - column;
	auto cells = static_cast<int>(text.size());
	if (cells > availableCells) {
		cells = availableCells;
	}
	if (fillToEnd || cells == 0) {
		cells = availableCells;
	}
	if (cells <= 0 || cells * cellWidth > maxLinePixels) {
		return false;
	}

	return _drawTextPixels(column * cellWidth, row * cellHeight, cells, text, style);
}

bool Screen::drawMenuItem(int row, std::string_view text, bool selected, Palette const& palette) {
	auto const style = selected
	    ? TextStyle{.foreground = palette.selectedForeground, .background = palette.selectedBackground}
	    : TextStyle{.foreground = palette.foreground, .background = palette.background};
	return drawText(0, row, text, style, true);
}

bool Screen::_drawTextPixels(int x, int y, int cells, std::string_view text, TextStyle style) {
	auto const lineWidth = cells * cellWidth;
	if (x < 0 || y < 0 || lineWidth <= 0 || x + lineWidth > width() || y + cellHeight > height()) {
		return false;
	}

	for (int sourceY = 0; sourceY < cellHeight; ++sourceY) {
		fillLine(_line.data(), lineWidth, style.background);
		for (int cell = 0; cell < cells; ++cell) {
			auto const character = cell < static_cast<int>(text.size()) ? text[static_cast<std::size_t>(cell)] : ' ';
			std::uint8_t glyph[glyphHeight]{};
			_glyphRows(character, glyph);
			auto const glyphRow = sourceY < glyphHeight ? glyph[sourceY] : std::uint8_t{0};
			auto* const cellPixels = _line.data() + cell * cellWidth;
			for (int sourceX = 0; sourceX < glyphWidth; ++sourceX) {
				if ((glyphRow & (1U << (glyphWidth - 1 - sourceX))) != 0) {
					cellPixels[sourceX] = style.foreground;
				}
			}
		}
		if (!_display.blitRgb565({.x = x, .y = y + sourceY, .width = lineWidth, .height = 1}, _line.data(), lineWidth)) {
			return false;
		}
	}
	return true;
}

void Screen::_glyphRows(char character, std::uint8_t (&rows)[glyphHeight]) {
	(void) lexxos::app::picocalc_lcd_console::detail::glyphRows(character, rows);
}

InputReader::InputReader(lexxos::devices::GamepadDevice* gamepad)
    : _gamepad(gamepad) {
}

void InputReader::reset() {
	lexxos::app::program_input::reset(_inputCursor);
	_previousGamepad = _gamepadState();
	_ready = true;
}

bool InputReader::poll(Action& action) {
	if (!_ready) {
		reset();
	}

	Action inputAction = Action::None;
	lexxos::devices::InputEvent event{};
	while (lexxos::app::program_input::pollEvent(_inputCursor, event)) {
		inputAction = _actionForInputEvent(event);
		if (inputAction != Action::None) {
			break;
		}
	}

	auto const currentGamepad = _gamepadState();
	auto const gamepadAction = _actionForGamepadEdge(_previousGamepad, currentGamepad);
	_previousGamepad = currentGamepad;

	if (gamepadAction == Action::Start || gamepadAction == Action::Select) {
		action = gamepadAction;
		return true;
	}
	if (inputAction != Action::None) {
		action = inputAction;
		return true;
	}
	if (gamepadAction != Action::None) {
		action = gamepadAction;
		return true;
	}

	action = Action::None;
	return false;
}

lexxos::devices::GamepadState InputReader::_gamepadState() const {
	return _gamepad != nullptr && _gamepad->isReady() ? _gamepad->state() : lexxos::devices::GamepadState{};
}

Action InputReader::_actionForInputEvent(lexxos::devices::InputEvent const& event) {
	if (event.type != lexxos::devices::InputEventType::KeyDown) {
		return Action::None;
	}

	using lexxos::devices::KeyCode;
	switch (event.key) {
		case KeyCode::Up:
			return Action::Up;
		case KeyCode::Down:
			return Action::Down;
		case KeyCode::Left:
			return Action::Left;
		case KeyCode::Right:
			return Action::Right;
		case KeyCode::Enter:
		case KeyCode::Space:
		case KeyCode::A:
		case KeyCode::Z:
		case KeyCode::Num1:
			return Action::Confirm;
		case KeyCode::Escape:
		case KeyCode::B:
		case KeyCode::X:
		case KeyCode::Num3:
			return Action::Back;
		case KeyCode::Tab:
			return Action::Start;
		case KeyCode::Backspace:
			return Action::Select;
		default:
			return Action::None;
	}
}

Action InputReader::_actionForGamepadEdge(
    lexxos::devices::GamepadState const& previous,
    lexxos::devices::GamepadState const& current) {
	if (pressed(previous.up, current.up)) {
		return Action::Up;
	}
	if (pressed(previous.down, current.down)) {
		return Action::Down;
	}
	if (pressed(previous.left, current.left)) {
		return Action::Left;
	}
	if (pressed(previous.right, current.right)) {
		return Action::Right;
	}
	if (pressed(previous.a, current.a) || pressed(previous.x, current.x)) {
		return Action::Confirm;
	}
	if (pressed(previous.b, current.b) || pressed(previous.y, current.y)) {
		return Action::Back;
	}
	if (pressed(previous.start, current.start)) {
		return Action::Start;
	}
	if (pressed(previous.select, current.select)) {
		return Action::Select;
	}
	return Action::None;
}

void MenuState::clamp(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		selected = 0;
		scroll = 0;
		return;
	}
	if (selected >= itemCount) {
		selected = itemCount - 1U;
	}
	if (visibleRows == 0) {
		scroll = 0;
		return;
	}
	if (selected < scroll) {
		scroll = selected;
	}
	if (selected >= scroll + visibleRows) {
		scroll = selected - visibleRows + 1U;
	}
	auto const maxScroll = itemCount > visibleRows ? itemCount - visibleRows : std::size_t{0};
	if (scroll > maxScroll) {
		scroll = maxScroll;
	}
}

bool MenuState::moveUp(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		return false;
	}
	selected = selected == 0 ? itemCount - 1U : selected - 1U;
	clamp(itemCount, visibleRows);
	return true;
}

bool MenuState::moveDown(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		return false;
	}
	selected = (selected + 1U) % itemCount;
	clamp(itemCount, visibleRows);
	return true;
}

} // namespace lexxos::tui
