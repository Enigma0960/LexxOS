#include <lexxos/devices/input/picocalc_keyboard_layout.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t keyBackspace = 0x08;
	constexpr std::uint8_t keyTab = 0x09;
	constexpr std::uint8_t keyEnterLf = 0x0A;
	constexpr std::uint8_t keyEnterCr = 0x0D;
	constexpr std::uint8_t keySpace = 0x20;
	constexpr std::uint8_t keyEscape = 0x1B;
	constexpr std::uint8_t keyDelete = 0x7F;
	constexpr std::uint8_t keyF1 = 0x81;
	constexpr std::uint8_t keyF9 = 0x89;
	constexpr std::uint8_t keyF10Legacy = 0x8A;
	constexpr std::uint8_t keyF10 = 0x90;
	constexpr std::uint8_t keyModAlt = 0xA1;
	constexpr std::uint8_t keyModShiftLeft = 0xA2;
	constexpr std::uint8_t keyModShiftRight = 0xA3;
	constexpr std::uint8_t keyModSym = 0xA4;
	constexpr std::uint8_t keyModCtrl = 0xA5;
	constexpr std::uint8_t keyEsc = 0xB1;
	constexpr std::uint8_t keyLeft = 0xB4;
	constexpr std::uint8_t keyUp = 0xB5;
	constexpr std::uint8_t keyDown = 0xB6;
	constexpr std::uint8_t keyRight = 0xB7;
	constexpr std::uint8_t keyBreak = 0xD0;
	constexpr std::uint8_t keyInsert = 0xD1;
	constexpr std::uint8_t keyHome = 0xD2;
	constexpr std::uint8_t keyDeletePicoCalc = 0xD4;
	constexpr std::uint8_t keyEnd = 0xD5;

	[[nodiscard]] bool shiftActive(std::uint8_t modifiers) {
		return (modifiers & (InputModifierShiftLeft | InputModifierShiftRight)) != 0;
	}

	[[nodiscard]] KeyCode mapShiftLayerKey(KeyCode key, std::uint8_t modifiers) {
		if (!shiftActive(modifiers)) {
			return key;
		}

		switch (key) {
			case KeyCode::F1:
				return KeyCode::F6;
			case KeyCode::F2:
				return KeyCode::F7;
			case KeyCode::F3:
				return KeyCode::F8;
			case KeyCode::F4:
				return KeyCode::F9;
			case KeyCode::F5:
				return KeyCode::F10;
			case KeyCode::Escape:
				return KeyCode::Break;
			case KeyCode::Tab:
				return KeyCode::Home;
			case KeyCode::Delete:
				return KeyCode::End;
			default:
				return key;
		}
	}

	[[nodiscard]] KeyCode mapLetter(std::uint8_t code) {
		constexpr KeyCode letters[] = {
		    KeyCode::A,
		    KeyCode::B,
		    KeyCode::C,
		    KeyCode::D,
		    KeyCode::E,
		    KeyCode::F,
		    KeyCode::G,
		    KeyCode::H,
		    KeyCode::I,
		    KeyCode::J,
		    KeyCode::K,
		    KeyCode::L,
		    KeyCode::M,
		    KeyCode::N,
		    KeyCode::O,
		    KeyCode::P,
		    KeyCode::Q,
		    KeyCode::R,
		    KeyCode::S,
		    KeyCode::T,
		    KeyCode::U,
		    KeyCode::V,
		    KeyCode::W,
		    KeyCode::X,
		    KeyCode::Y,
		    KeyCode::Z,
		};
		if (code >= 'a' && code <= 'z') {
			return letters[code - 'a'];
		}
		if (code >= 'A' && code <= 'Z') {
			return letters[code - 'A'];
		}
		return KeyCode::Unknown;
	}

	[[nodiscard]] KeyCode mapNumber(std::uint8_t code) {
		constexpr KeyCode numbers[] = {
		    KeyCode::Num0,
		    KeyCode::Num1,
		    KeyCode::Num2,
		    KeyCode::Num3,
		    KeyCode::Num4,
		    KeyCode::Num5,
		    KeyCode::Num6,
		    KeyCode::Num7,
		    KeyCode::Num8,
		    KeyCode::Num9,
		};
		if (code >= '0' && code <= '9') {
			return numbers[code - '0'];
		}

		switch (code) {
			case ')':
				return KeyCode::Num0;
			case '!':
				return KeyCode::Num1;
			case '@':
				return KeyCode::Num2;
			case '#':
				return KeyCode::Num3;
			case '$':
				return KeyCode::Num4;
			case '%':
				return KeyCode::Num5;
			case '^':
				return KeyCode::Num6;
			case '&':
				return KeyCode::Num7;
			case '*':
				return KeyCode::Num8;
			case '(':
				return KeyCode::Num9;
			default:
				return KeyCode::Unknown;
		}
	}

	[[nodiscard]] KeyCode mapPunctuation(std::uint8_t code) {
		switch (code) {
			case '`':
			case '~':
				return KeyCode::Grave;
			case '-':
			case '_':
				return KeyCode::Minus;
			case '=':
			case '+':
				return KeyCode::Equals;
			case '[':
			case '{':
				return KeyCode::LeftBracket;
			case ']':
			case '}':
				return KeyCode::RightBracket;
			case '\\':
			case '|':
				return KeyCode::Backslash;
			case ';':
			case ':':
				return KeyCode::Semicolon;
			case '\'':
			case '"':
				return KeyCode::Apostrophe;
			case ',':
			case '<':
				return KeyCode::Comma;
			case '.':
			case '>':
				return KeyCode::Period;
			case '/':
			case '?':
				return KeyCode::Slash;
			default:
				return KeyCode::Unknown;
		}
	}

	[[nodiscard]] KeyCode mapBaseKey(std::uint8_t code) {
		switch (code) {
			case keyBackspace:
				return KeyCode::Backspace;
			case keyTab:
				return KeyCode::Tab;
			case keyEnterLf:
			case keyEnterCr:
				return KeyCode::Enter;
			case keySpace:
				return KeyCode::Space;
			case keyEscape:
			case keyEsc:
				return KeyCode::Escape;
			case keyDelete:
			case keyDeletePicoCalc:
				return KeyCode::Delete;
			case keyBreak:
				return KeyCode::Break;
			case keyInsert:
				return KeyCode::Insert;
			case keyHome:
				return KeyCode::Home;
			case keyEnd:
				return KeyCode::End;
			case keyLeft:
				return KeyCode::Left;
			case keyUp:
				return KeyCode::Up;
			case keyDown:
				return KeyCode::Down;
			case keyRight:
				return KeyCode::Right;
			case keyF10:
			case keyF10Legacy:
				return KeyCode::F10;
			default:
				break;
		}

		if (code >= keyF1 && code <= keyF9) {
			return static_cast<KeyCode>(static_cast<std::uint16_t>(KeyCode::F1) + code - keyF1);
		}

		auto key = mapLetter(code);
		if (key != KeyCode::Unknown) {
			return key;
		}

		key = mapNumber(code);
		if (key != KeyCode::Unknown) {
			return key;
		}

		return mapPunctuation(code);
	}

	[[nodiscard]] std::uint8_t eventCharacter(std::uint8_t code) {
		if (code >= 32 && code <= 126) {
			return code;
		}

		switch (code) {
			case keyBackspace:
			case keyTab:
			case keyEnterLf:
			case keyEnterCr:
				return code;
			default:
				return 0;
		}
	}

} // namespace

PicoCalcKeyboardMapping picoCalcKeyboardMapScanCode(std::uint8_t scanCode, std::uint8_t modifiers) {
	auto const modifierMask = picoCalcKeyboardModifierMask(scanCode);
	if (modifierMask != 0) {
		return {
		    .key = KeyCode::Unknown,
		    .character = 0,
		    .modifierMask = modifierMask,
		};
	}

	return {
	    .key = mapShiftLayerKey(mapBaseKey(scanCode), modifiers),
	    .character = eventCharacter(scanCode),
	    .modifierMask = 0,
	};
}

bool picoCalcKeyboardIsModifierScanCode(std::uint8_t scanCode) {
	return picoCalcKeyboardModifierMask(scanCode) != 0;
}

std::uint8_t picoCalcKeyboardModifierMask(std::uint8_t scanCode) {
	switch (scanCode) {
		case keyModAlt:
			return InputModifierAlt;
		case keyModShiftLeft:
			return InputModifierShiftLeft;
		case keyModShiftRight:
			return InputModifierShiftRight;
		case keyModSym:
			return InputModifierSym;
		case keyModCtrl:
			return InputModifierCtrl;
		default:
			return 0;
	}
}

bool picoCalcKeyboardIsRepeatable(PicoCalcKeyboardMapping mapping, std::uint8_t scanCode) {
	if ((scanCode >= keyF1 && scanCode <= keyF9) || scanCode == keyF10 || scanCode == keyF10Legacy) {
		return false;
	}
	if (scanCode == keyEsc) {
		return false;
	}

	switch (mapping.key) {
		case KeyCode::Left:
		case KeyCode::Right:
		case KeyCode::Up:
		case KeyCode::Down:
		case KeyCode::Backspace:
		case KeyCode::Space:
			return true;
		default:
			return false;
	}
}

} // namespace lexxos::devices
