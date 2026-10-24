#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::picocalc_programs {

/**
 * @lexxos-command-doc beep
 * @boards picocalc_pico_w picocalc_pico2w
 * @source src/lexxos/apps/picocalc/beep_program.cpp
 * @description
 * Проигрывает короткий PWM-сигнал через аудиовыводы, описанные профилем платы. На PicoCalc команда последовательно
 * проверяет левый канал GP26, делает короткую паузу и проверяет правый канал GP27.
 *
 * @parameters
 * - `--frequency <hz>` / `-f <hz>` — необязательная частота; по умолчанию берётся из профиля платы; диапазон
 *   `20..20000`.
 * - `--duration <ms>` / `-d <ms>` — необязательная длительность; по умолчанию берётся из профиля платы; диапазон
 *   `1..5000`.
 *
 * @examples
 * ```text
 * lexx:/> beep
 * beep: left pin=26
 * beep: right pin=27
 *
 * lexx:/> beep --frequency 1200 --duration 200
 * ```
 *
 * @author
 * `beep` остаётся простой диагностикой распиновки платы. Для пользовательского звукового пути предпочтительнее команда
 * `audio` и `AudioDevice`, потому что они не требуют прямого доступа приложения к GPIO/PWM.
 * @end
 */
class BeepProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::picocalc_programs
