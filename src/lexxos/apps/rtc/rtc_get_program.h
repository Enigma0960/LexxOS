#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs {

/**
 * @lexxos-command-doc rtc-get
 * @source src/lexxos/apps/rtc/rtc_get_program.cpp
 * @description
 * Читает дату и время из выбранного RTC backend и печатает источник, значение RTC и weekday. Если oscillator-stop,
 * voltage-low или software-valid flag требует внимания, команда предупреждает, что время может быть недостоверным.
 *
 * @parameters
 * - `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
 * - `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
 * - `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.
 *
 * @examples
 * ```text
 * lexx:/> rtc-get
 * lexx:/> rtc-get --source software
 * lexx:/> rtc-get --source picocalc
 * lexx:/> rtc-get --source pcf8563 --bus system
 * ```
 *
 * @author
 * Используйте `rtc-get` после boot и после `rtc-set`, чтобы проверить источник времени для PicoCalc status bar.
 * @end
 */
class RtcGetProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::rtc_programs
