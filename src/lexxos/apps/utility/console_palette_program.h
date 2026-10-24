#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc console-palette
 * @source src/lexxos/apps/utility/console_palette_program.cpp
 * @description
 * Печатает ANSI SGR образцы foreground/background цветов. UART-terminal получает обычные escape-последовательности,
 * а LCD console mirror разбирает тот же поток и рисует packed 16-color RGB565 palette.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> console-palette
 * ```
 *
 * @author
 * Используйте эту команду как smoke-test после изменений terminal-state или renderer: цвета должны отличаться и на UART,
 * и на LCD mirror. Если UART terminal не показывает цвета, проверьте, что клиент не фильтрует ANSI SGR.
 * @end
 */
class ConsolePaletteProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::utility_programs
