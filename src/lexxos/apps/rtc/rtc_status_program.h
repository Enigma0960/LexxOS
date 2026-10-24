#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs {

/**
 * @lexxos-command-doc rtc-status
 * @source src/lexxos/apps/rtc/rtc_status_program.cpp
 * @description
 * Читает статус выбранного RTC backend: источник, raw status byte, oscillator-stop/voltage-low flags, software RTC
 * snapshot и дополнительные поля JCS firmware, когда они доступны.
 *
 * @parameters
 * - `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
 * - `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
 * - `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.
 *
 * @examples
 * ```text
 * lexx:/> rtc-status
 * lexx:/> rtc-status --source software
 * lexx:/> rtc-status --source picocalc
 * lexx:/> rtc-status --source ds3231 --bus debug --address 0x68
 * lexx:/> rtc-status --source pcf8563 --bus system
 * ```
 *
 * @author
 * На PicoCalc JCS firmware `auto` выбирает southbridge RTC. DS3231 и PCF8563/BM8563 остаются отдельными I2C backend-ами.
 * Software RTC должен быть явно включен в профиле платы.
 * @end
 */
class RtcStatusProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::rtc_programs
