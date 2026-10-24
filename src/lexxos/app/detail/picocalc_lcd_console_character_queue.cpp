#include <lexxos/app/detail/picocalc_lcd_console_character_queue.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleCharacterQueue::reset() {
	_head = 0;
	_tail = 0;
	_count = 0;
}

void LcdConsoleCharacterQueue::enqueue(char value) {
	if (_count >= _buffer.size()) {
		_tail = (_tail + 1U) % _buffer.size();
		--_count;
	}
	_buffer[_head] = value;
	_head = (_head + 1U) % _buffer.size();
	++_count;
}

bool LcdConsoleCharacterQueue::dequeue(char& value) {
	if (_count == 0) {
		return false;
	}

	value = _buffer[_tail];
	_tail = (_tail + 1U) % _buffer.size();
	--_count;
	return true;
}

bool LcdConsoleCharacterQueue::hasCharacters() const {
	return _count != 0;
}

std::size_t LcdConsoleCharacterQueue::count() const {
	return _count;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
