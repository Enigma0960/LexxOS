#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc power
 * @source src/lexxos/apps/utility/power_program.cpp
 * @description
 * Показывает состояние питания платы, батареи и поддержки программного выключения. На PicoCalc команда читает
 * service-backed snapshot `picocalc-southbridge` без прямого I2C-доступа из shell.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> power
 * power: source=picocalc-southbridge direct-i2c=no
 * ```
 *
 * @author
 * `power` должен оставаться read-only диагностикой. Для изменения подсветки используйте `backlight`, для режима дисплея
 * `display-power`, а для выключения устройства `poweroff`.
 * @end
 */
class PowerProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::utility_programs
