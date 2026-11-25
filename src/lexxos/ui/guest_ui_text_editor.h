#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/ui/controls/text_field.h>

namespace lexxos::ui {

// Text caret and viewport are host-side state, never serialized to the guest.
class TextEditor final {
public:
	void reset(Control const& control) {
		_cursor = std::strlen(control.text);
		_first = 0;
	}

	[[nodiscard]] std::size_t cursor() const { return _cursor; }

	[[nodiscard]] std::size_t first() const { return _first; }

	void clamp(Control const& control) {
		auto const length = std::strlen(control.text);
		_cursor = std::min(_cursor, length);
		while (_cursor && _cursor < length && continuation(control.text[_cursor])) {
			--_cursor;
		}

		_first = std::min(_first, _cursor);
		while (_first && continuation(control.text[_first])) {
			--_first;
		}
	}

	bool key(Control& control, devices::KeyCode code, char character, Event& event, bool& dirty) {
		clamp(control);
		auto const length = std::strlen(control.text);
		bool changed = false;
		switch (code) {
			case devices::KeyCode::Left:
				if (_cursor) {
					_cursor = previous(control.text, _cursor);
					dirty = true;
				}

				return true;
			case devices::KeyCode::Right:
				if (_cursor < length) {
					_cursor = next(control.text, length, _cursor);
					dirty = true;
				}

				return true;
			case devices::KeyCode::Home:
				if (_cursor) {
					_cursor = 0;
					dirty = true;
				}

				return true;
			case devices::KeyCode::End:
				if (_cursor != length) {
					_cursor = length;
					dirty = true;
				}

				return true;
			case devices::KeyCode::Backspace:
				if (_cursor) {
					auto const start = previous(control.text, _cursor);
					std::memmove(control.text + start, control.text + _cursor, length - _cursor + 1);
					_cursor = start;
					changed = true;
				}

				break;
			case devices::KeyCode::Delete:
				if (_cursor < length) {
					auto const end = next(control.text, length, _cursor);
					std::memmove(control.text + _cursor, control.text + end, length - end + 1);
					changed = true;
				}

				break;
			default:
				if ((character == '\b' || static_cast<unsigned char>(character) == 0x7F) && _cursor) {
					auto const start = previous(control.text, _cursor);
					std::memmove(control.text + start, control.text + _cursor, length - _cursor + 1);
					_cursor = start;
					changed = true;
				} else if (character >= 32 && character <= 126 && length < sizeof(control.text) - 1) {
					std::memmove(control.text + _cursor + 1, control.text + _cursor, length - _cursor + 1);
					control.text[_cursor++] = character;
					changed = true;
				}

				break;
		}

		if (!changed) {
			return false;
		}

		dirty = true;
		event.type = 2;
		event.id = control.id;
		std::memcpy(event.text, control.text, sizeof(event.text));
		return true;
	}

	// The glyph-aware viewport follows the cursor in either direction.
	void adjustViewport(Control const& control) {
		clamp(control);
		int const capacity = std::max(0, (control.width - 9) / static_cast<int>(font::cellWidth));
		if (_cursor < _first) {
			_first = _cursor;
		}

		int visible = count(control.text, _first, _cursor);
		while (visible > capacity) {
			_first = next(control.text, std::strlen(control.text), _first);
			--visible;
		}
	}

	[[nodiscard]] int cursorX(Control const& control) const {
		return control.x + 5 + count(control.text, _first, _cursor) * static_cast<int>(font::cellWidth);
	}

	bool render(devices::DisplayDevice& display, Control const& control) const {
		if (!controls::drawPanel(display, control, true)) {
			return false;
		}

		int const capacity = std::max(0, (control.width - 9) / static_cast<int>(font::cellWidth));
		char const* const begin = control.text + _first;
		auto end = _first;
		auto const length = std::strlen(control.text);
		for (int i = 0; i < capacity && end < length; ++i) {
			end = next(control.text, length, end);
		}

		int const y = control.y + (control.height - static_cast<int>(font::cellHeight)) / 2;
		if (!drawText(display, control.x + 5, y, std::string_view(begin, end - _first),
		        canvas, accent, control.width - 7)) {
			return false;
		}

		return display.fillRgb565({cursorX(control), y, 2, static_cast<int>(font::cellHeight)}, canvas);
	}

private:
	static bool continuation(char c) { return (static_cast<std::uint8_t>(c) & 0xC0U) == 0x80U; }

	static std::size_t previous(char const* text, std::size_t offset) {
		if (!offset) {
			return 0;
		}

		do {
			--offset;
		} while (offset && continuation(text[offset]));

		return offset;
	}

	static std::size_t next(char const* text, std::size_t length, std::size_t offset) {
		if (offset >= length) {
			return length;
		}

		++offset;
		while (offset < length && continuation(text[offset])) {
			++offset;
		}

		return offset;
	}

	static int count(char const* text, std::size_t first, std::size_t end) {
		int result = 0;
		while (first < end) {
			first = next(text, end, first);
			++result;
		}

		return result;
	}

	std::size_t _cursor{0};
	std::size_t _first{0};
};

} // namespace lexxos::ui
