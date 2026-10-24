#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc backlight
 * @source src/lexxos/apps/utility/backlight_program.cpp
 * @description
 * Показывает или меняет уровни LCD/keyboard подсветки через board-level power/backlight service. На PicoCalc команда
 * использует `PicoCalcSouthbridgeService`, а на дисплеях с device-level backlight управляет подсветкой через
 * `DisplayDevice`.
 *
 * @parameters
 * - Без параметров — показать текущие уровни подсветки.
 * - `lcd <0..255>` / `kbd <0..255>` — установить уровень LCD или клавиатурной подсветки.
 * - `save [lcd|kbd|all]` — сохранить текущие уровни на SD.
 * - `restore [lcd|kbd|all]` / `load [lcd|kbd|all]` — восстановить сохранённые уровни.
 *
 * @examples
 * ```text
 * lexx:/> backlight
 * lexx:/> backlight lcd 40
 * lexx:/> backlight kbd 10
 * lexx:/> backlight save all
 * ```
 *
 * @author
 * Это основной пользовательский путь для подсветки. `save` пишет `/mnt/sd/.lexx/backlight.conf`, а boot runtime
 * автоматически применяет этот файл после успешного SD automount на PicoCalc. Диагностические записи через
 * `picocalc-sb` намеренно заблокированы при активной клавиатуре, чтобы не повредить состояние southbridge FIFO.
 * @end
 */
class BacklightProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::utility_programs
