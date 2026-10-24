#pragma once

#include <cstdint>

namespace lexxos::io {
class OutputStream;
} // namespace lexxos::io

namespace lexxos::terminal {

enum class KeyCode : std::uint8_t {
	Unknown,
	Character,
	Enter,
	Backspace,
	Delete,
	Escape,
	ArrowUp,
	ArrowDown,
	ArrowRight,
	ArrowLeft,
	Home,
	End,
	PageUp,
	PageDown,
	CtrlC,
	CtrlQ,
	CtrlS,
	CtrlX,
};

struct Key {
	KeyCode code{KeyCode::Unknown};
	char character{0};
};

[[nodiscard]] bool isPrintableAscii(Key const& key);
[[nodiscard]] Key readKey();

void clearScreen(lexxos::io::OutputStream& output);
void eraseLine(lexxos::io::OutputStream& output);
void moveCursor(lexxos::io::OutputStream& output, std::uint8_t row, std::uint8_t column);
void hideCursor(lexxos::io::OutputStream& output);
void showCursor(lexxos::io::OutputStream& output);

} // namespace lexxos::terminal
