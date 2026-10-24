#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs {

/**
 * @lexxos-command-doc rtc-set
 * @source src/lexxos/apps/rtc/rtc_set_program.cpp
 * @description
 * Записывает дату и время в выбранный RTC backend. Общий диапазон LexxOS — `2000-01-01..2199-12-31`; PicoCalc JCS
 * RTC поддерживает `2000..2099`, software RTC хранит время в RAM до выключения питания и использует Unix range.
 *
 * @parameters
 * - `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
 * - `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
 * - `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.
 * - `<YYYY-MM-DD> <HH:MM:SS>` — обязательные дата и время; также допускается ISO-like первый аргумент
 *   `YYYY-MM-DDTHH:MM:SS`.
 *
 * @examples
 * ```text
 * lexx:/> rtc-set 2026-06-23 14:30:00
 * lexx:/> rtc-set --source software 2026-06-23 14:30:00
 * lexx:/> rtc-set --source picocalc 2026-06-23T14:30:00
 * lexx:/> rtc-set --source pcf8563 --bus system 2026-06-23 14:30:00
 * ```
 *
 * @author
 * После записи сразу проверьте `rtc-get` и `console-info`: status bar должен увидеть обновлённое время через тот же
 * RTC/southbridge путь.
 * @end
 */
class RtcSetProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::rtc_programs
