#pragma once

#include <array>
#include <cstddef>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleCsiState final {
public:
	void reset();

	[[nodiscard]] bool consume(char value);
	[[nodiscard]] bool isFinal(char value) const;

	void finishParameter();

	[[nodiscard]] int parameter(std::size_t index, int defaultValue) const;
	[[nodiscard]] int parameterCount() const;
	[[nodiscard]] int countParameter() const;
	[[nodiscard]] bool isPrivate() const;

private:
	std::array<int, maxCsiParameters> _parameters{};
	std::size_t _parameterCount{0};
	int _currentValue{0};
	bool _currentPresent{false};
	bool _private{false};
	bool _ignore{false};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
