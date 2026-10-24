#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::i2c_programs {

/**
 * @lexxos-command-doc i2c-buses
 * @source src/lexxos/apps/i2c_buses_program.cpp
 * @description
 * Показывает I2C-шины, зарегистрированные в `DeviceManager`. Команда помогает понять, какие имена шин доступны для
 * `i2c-scan`, RTC-команд и низкоуровневой диагностики первичного запуска.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> i2c-buses
 * ```
 *
 * @author
 * На PicoCalc основная southbridge-шина обычно называется `debug`; конкретные имена должны проверяться этой командой,
 * а не дублироваться в документации вручную.
 * @end
 */
class I2cBusesProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::i2c_programs
