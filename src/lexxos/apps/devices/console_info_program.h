#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs {

/**
 * @lexxos-command-doc console-info
 * @source src/lexxos/apps/devices/console_info_program.cpp
 * @description
 * Печатает состояние LCD console: геометрию, cursor, status bar, scrollback и аппаратный scroll state.
 * Команда помогает проверять standalone shell/editor поведение без внешнего UART.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> console-info
 * ```
 *
 * @author
 * Используйте `console-info` после проверки status bar, RTC/battery snapshot и LCD scrollback hotkeys, чтобы
 * зафиксировать фактическое состояние консоли в smoke-test.
 * @end
 */
class ConsoleInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::device_programs
