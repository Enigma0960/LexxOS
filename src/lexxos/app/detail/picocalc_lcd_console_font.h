#pragma once

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

bool glyphRows(char character, std::uint8_t (&rows)[glyphHeight]);

} // namespace lexxos::app::picocalc_lcd_console::detail
