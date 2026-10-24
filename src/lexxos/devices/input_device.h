#pragma once

#include <cstdint>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

enum class InputEventType : std::uint8_t {
	KeyDown,
	KeyUp,
};

[[nodiscard]] constexpr std::string_view toString(InputEventType type) {
	switch (type) {
		case InputEventType::KeyDown:
			return "key-down";
		case InputEventType::KeyUp:
			return "key-up";
	}
	return "unknown";
}

enum class KeyCode : std::uint16_t {
	Unknown,
	Up,
	Down,
	Left,
	Right,
	Enter,
	Escape,
	Backspace,
	Space,
	A,
	B,
	C,
	D,
	X,
	Y,
	Z,
	Num0,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	Num7,
	Num8,
	Num9,
	Soft1,
	Soft2,
	Star,
	Sharp,
	Power,
	Reserved,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	Tab,
	Grave,
	Minus,
	Equals,
	LeftBracket,
	RightBracket,
	Backslash,
	Semicolon,
	Apostrophe,
	Comma,
	Period,
	Slash,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	Break,
	Home,
	End,
	Delete,
	Insert,
};

[[nodiscard]] constexpr std::string_view toString(KeyCode key) {
	switch (key) {
		case KeyCode::Unknown:
			return "unknown";
		case KeyCode::Up:
			return "up";
		case KeyCode::Down:
			return "down";
		case KeyCode::Left:
			return "left";
		case KeyCode::Right:
			return "right";
		case KeyCode::Enter:
			return "enter";
		case KeyCode::Escape:
			return "escape";
		case KeyCode::Backspace:
			return "backspace";
		case KeyCode::Space:
			return "space";
		case KeyCode::A:
			return "a";
		case KeyCode::B:
			return "b";
		case KeyCode::C:
			return "c";
		case KeyCode::D:
			return "d";
		case KeyCode::E:
			return "e";
		case KeyCode::F:
			return "f";
		case KeyCode::G:
			return "g";
		case KeyCode::H:
			return "h";
		case KeyCode::I:
			return "i";
		case KeyCode::J:
			return "j";
		case KeyCode::K:
			return "k";
		case KeyCode::L:
			return "l";
		case KeyCode::M:
			return "m";
		case KeyCode::N:
			return "n";
		case KeyCode::O:
			return "o";
		case KeyCode::P:
			return "p";
		case KeyCode::Q:
			return "q";
		case KeyCode::R:
			return "r";
		case KeyCode::S:
			return "s";
		case KeyCode::T:
			return "t";
		case KeyCode::U:
			return "u";
		case KeyCode::V:
			return "v";
		case KeyCode::W:
			return "w";
		case KeyCode::X:
			return "x";
		case KeyCode::Y:
			return "y";
		case KeyCode::Z:
			return "z";
		case KeyCode::Num0:
			return "0";
		case KeyCode::Num1:
			return "1";
		case KeyCode::Num2:
			return "2";
		case KeyCode::Num3:
			return "3";
		case KeyCode::Num4:
			return "4";
		case KeyCode::Num5:
			return "5";
		case KeyCode::Num6:
			return "6";
		case KeyCode::Num7:
			return "7";
		case KeyCode::Num8:
			return "8";
		case KeyCode::Num9:
			return "9";
		case KeyCode::Soft1:
			return "soft1";
		case KeyCode::Soft2:
			return "soft2";
		case KeyCode::Star:
			return "star";
		case KeyCode::Sharp:
			return "sharp";
		case KeyCode::Power:
			return "power";
		case KeyCode::Reserved:
			return "reserved";
		case KeyCode::Tab:
			return "tab";
		case KeyCode::Grave:
			return "grave";
		case KeyCode::Minus:
			return "minus";
		case KeyCode::Equals:
			return "equals";
		case KeyCode::LeftBracket:
			return "left-bracket";
		case KeyCode::RightBracket:
			return "right-bracket";
		case KeyCode::Backslash:
			return "backslash";
		case KeyCode::Semicolon:
			return "semicolon";
		case KeyCode::Apostrophe:
			return "apostrophe";
		case KeyCode::Comma:
			return "comma";
		case KeyCode::Period:
			return "period";
		case KeyCode::Slash:
			return "slash";
		case KeyCode::F1:
			return "f1";
		case KeyCode::F2:
			return "f2";
		case KeyCode::F3:
			return "f3";
		case KeyCode::F4:
			return "f4";
		case KeyCode::F5:
			return "f5";
		case KeyCode::F6:
			return "f6";
		case KeyCode::F7:
			return "f7";
		case KeyCode::F8:
			return "f8";
		case KeyCode::F9:
			return "f9";
		case KeyCode::F10:
			return "f10";
		case KeyCode::Break:
			return "break";
		case KeyCode::Home:
			return "home";
		case KeyCode::End:
			return "end";
		case KeyCode::Delete:
			return "delete";
		case KeyCode::Insert:
			return "insert";
	}
	return "unknown";
}

enum InputModifier : std::uint8_t {
	InputModifierAlt = 1U << 0U,
	InputModifierShiftLeft = 1U << 1U,
	InputModifierShiftRight = 1U << 2U,
	InputModifierSym = 1U << 3U,
	InputModifierCtrl = 1U << 4U,
};

struct InputEvent {
	InputEventType type{InputEventType::KeyDown};
	KeyCode key{KeyCode::Unknown};
	std::uint32_t timestampMs{0};
	std::uint8_t character{0};
	std::uint8_t scanCode{0};
	std::uint8_t modifiers{0};
};

class InputDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override;

	virtual void update();
	virtual void setRawEventReporting(bool enabled);
	virtual bool pollEvent(InputEvent& event) = 0;
	[[nodiscard]] virtual bool isPressed(KeyCode key) const = 0;

	[[nodiscard]] virtual std::uint32_t rawState() const;
	[[nodiscard]] virtual std::uint32_t rawKeyCount() const;
	[[nodiscard]] virtual std::uint32_t rawRowCount() const;
	[[nodiscard]] virtual std::uint32_t rawColumnCount() const;
};

} // namespace lexxos::devices
