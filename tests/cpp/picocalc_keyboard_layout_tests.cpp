#include <lexxos/devices/input/picocalc_keyboard_layout.h>

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "picocalc keyboard layout test failed: " << message << '\n';
		std::exit(1);
	}
}

void expectMapping(std::uint8_t scanCode, std::uint8_t modifiers, lexxos::devices::KeyCode key, std::uint8_t character) {
	auto const mapping = lexxos::devices::picoCalcKeyboardMapScanCode(scanCode, modifiers);
	expect(mapping.key == key, "unexpected mapped key");
	expect(mapping.character == character, "unexpected mapped character");
	expect(mapping.modifierMask == 0, "non-modifier mapping must not carry a modifier mask");
}

void checkPrintableMappings() {
	using lexxos::devices::KeyCode;

	expectMapping('a', 0, KeyCode::A, 'a');
	expectMapping('Z', 0, KeyCode::Z, 'Z');
	expectMapping('0', 0, KeyCode::Num0, '0');
	expectMapping('9', 0, KeyCode::Num9, '9');
	expectMapping('!', 0, KeyCode::Num1, '!');
	expectMapping('(', 0, KeyCode::Num9, '(');
	expectMapping('`', 0, KeyCode::Grave, '`');
	expectMapping('~', 0, KeyCode::Grave, '~');
	expectMapping('-', 0, KeyCode::Minus, '-');
	expectMapping('_', 0, KeyCode::Minus, '_');
	expectMapping('=', 0, KeyCode::Equals, '=');
	expectMapping('+', 0, KeyCode::Equals, '+');
	expectMapping('[', 0, KeyCode::LeftBracket, '[');
	expectMapping('{', 0, KeyCode::LeftBracket, '{');
	expectMapping(']', 0, KeyCode::RightBracket, ']');
	expectMapping('}', 0, KeyCode::RightBracket, '}');
	expectMapping('\\', 0, KeyCode::Backslash, '\\');
	expectMapping('|', 0, KeyCode::Backslash, '|');
	expectMapping(';', 0, KeyCode::Semicolon, ';');
	expectMapping(':', 0, KeyCode::Semicolon, ':');
	expectMapping('\'', 0, KeyCode::Apostrophe, '\'');
	expectMapping('"', 0, KeyCode::Apostrophe, '"');
	expectMapping(',', 0, KeyCode::Comma, ',');
	expectMapping('<', 0, KeyCode::Comma, '<');
	expectMapping('.', 0, KeyCode::Period, '.');
	expectMapping('>', 0, KeyCode::Period, '>');
	expectMapping('/', 0, KeyCode::Slash, '/');
	expectMapping('?', 0, KeyCode::Slash, '?');
}

void checkControlAndSystemMappings() {
	using lexxos::devices::InputModifierShiftLeft;
	using lexxos::devices::KeyCode;

	expectMapping(0x08, 0, KeyCode::Backspace, 0x08);
	expectMapping(0x09, 0, KeyCode::Tab, 0x09);
	expectMapping(0x0A, 0, KeyCode::Enter, 0x0A);
	expectMapping(0x0D, 0, KeyCode::Enter, 0x0D);
	expectMapping(0x20, 0, KeyCode::Space, 0x20);
	expectMapping(0x1B, 0, KeyCode::Escape, 0);
	expectMapping(0x7F, 0, KeyCode::Delete, 0);

	expectMapping(0x81, 0, KeyCode::F1, 0);
	expectMapping(0x85, 0, KeyCode::F5, 0);
	expectMapping(0x89, 0, KeyCode::F9, 0);
	expectMapping(0x8A, 0, KeyCode::F10, 0);
	expectMapping(0x90, 0, KeyCode::F10, 0);

	expectMapping(0x81, InputModifierShiftLeft, KeyCode::F6, 0);
	expectMapping(0x85, InputModifierShiftLeft, KeyCode::F10, 0);
	expectMapping(0xB1, InputModifierShiftLeft, KeyCode::Break, 0);
	expectMapping(0x09, InputModifierShiftLeft, KeyCode::Home, 0x09);
	expectMapping(0x7F, InputModifierShiftLeft, KeyCode::End, 0);

	expectMapping(0xB4, 0, KeyCode::Left, 0);
	expectMapping(0xB5, 0, KeyCode::Up, 0);
	expectMapping(0xB6, 0, KeyCode::Down, 0);
	expectMapping(0xB7, 0, KeyCode::Right, 0);
	expectMapping(0xD0, 0, KeyCode::Break, 0);
	expectMapping(0xD1, 0, KeyCode::Insert, 0);
	expectMapping(0xD2, 0, KeyCode::Home, 0);
	expectMapping(0xD4, 0, KeyCode::Delete, 0);
	expectMapping(0xD5, 0, KeyCode::End, 0);
}

void checkModifiers() {
	using lexxos::devices::InputModifierAlt;
	using lexxos::devices::InputModifierCtrl;
	using lexxos::devices::InputModifierShiftLeft;
	using lexxos::devices::InputModifierShiftRight;
	using lexxos::devices::InputModifierSym;
	using lexxos::devices::KeyCode;

	constexpr std::uint8_t modifiers[][2] = {
	    {0xA1,        InputModifierAlt},
	    {0xA2,  InputModifierShiftLeft},
	    {0xA3, InputModifierShiftRight},
	    {0xA4,        InputModifierSym},
	    {0xA5,       InputModifierCtrl},
	};

	for (auto const entry : modifiers) {
		expect(lexxos::devices::picoCalcKeyboardIsModifierScanCode(entry[0]), "modifier scan-code expected");
		expect(lexxos::devices::picoCalcKeyboardModifierMask(entry[0]) == entry[1], "modifier mask mismatch");
		auto const mapping = lexxos::devices::picoCalcKeyboardMapScanCode(entry[0], 0);
		expect(mapping.key == KeyCode::Unknown, "modifier should not map to a key event");
		expect(mapping.character == 0, "modifier should not map to a character");
		expect(mapping.modifierMask == entry[1], "modifier mapping should carry the mask");
	}

	expect(!lexxos::devices::picoCalcKeyboardIsModifierScanCode('a'), "letter must not be a modifier");
	expect(lexxos::devices::picoCalcKeyboardModifierMask('a') == 0, "letter modifier mask must be zero");
}

void checkRepeatableMappings() {
	using lexxos::devices::picoCalcKeyboardIsRepeatable;
	using lexxos::devices::picoCalcKeyboardMapScanCode;

	expect(picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0xB4), 0xB4), "left should repeat");
	expect(picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0xB7), 0xB7), "right should repeat");
	expect(picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0x08), 0x08), "backspace should repeat");
	expect(picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0x20), 0x20), "space should repeat");
	expect(!picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode('a'), 'a'), "printable letters should not repeat");
	expect(!picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0x81), 0x81), "function keys should not repeat");
	expect(!picoCalcKeyboardIsRepeatable(picoCalcKeyboardMapScanCode(0xB1), 0xB1), "escape scan-code should not repeat");
}

} // namespace

void checkPicoCalcKeyboardLayout() {
	checkPrintableMappings();
	checkControlAndSystemMappings();
	checkModifiers();
	checkRepeatableMappings();
}
