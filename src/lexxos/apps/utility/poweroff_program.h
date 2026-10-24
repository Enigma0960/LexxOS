#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc poweroff
 * @source src/lexxos/apps/utility/poweroff_program.cpp
 * @description
 * Синхронизирует и размонтирует writable файловые системы, затем запрашивает выключение платы, если профиль устройства
 * поддерживает board-level power-off. На PicoCalc запрос идёт через `picocalc-southbridge` service.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> poweroff
 * ```
 *
 * @author
 * На проверенной JCS-прошивке PicoCalc VFS sync/unmount проходит, но southbridge request может вернуть
 * `southbridge request failed`, и устройство продолжает работать. Это задокументированное ограничение 0.0.7.
 * @end
 */
class PoweroffProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::utility_programs
