#pragma once

#include <array>
#include <cstddef>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleCharacterQueue {
public:
	void reset();

	void enqueue(char value);

	bool dequeue(char& value);

	[[nodiscard]] bool hasCharacters() const;

	[[nodiscard]] std::size_t count() const;

private:
	std::array<char, queueCapacity> _buffer{};
	std::size_t _head{0};
	std::size_t _tail{0};
	std::size_t _count{0};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
