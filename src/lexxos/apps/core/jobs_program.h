#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::system_programs {

/**
 * @lexxos-command-doc jobs
 * @source src/lexxos/apps/core/jobs_program.cpp
 * @description
 * Показывает таблицу процессов `ProgramSupervisor`: id, состояние, имя программы, размер стека, свободный стек,
 * priority, memory class, внешнюю квоту arena, exit code и launch status.
 *
 * @parameters
 * - Без параметров — показать все активные и завершённые supervised processes.
 * - `clear` — удалить из таблицы завершённые процессы.
 *
 * @examples
 * ```text
 * lexx:/> jobs
 * lexx:/> jobs clear
 * ```
 *
 * @author
 * `jobs` сейчас является главным диагностическим окном для параллельной модели: по `stack free` подбирается
 * `ProgramRuntimeProfile::stackSizeBytes`, а поля `int`/`ext` показывают зарезервированные/запрошенные квоты arena.
 * @end
 */
class JobsProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::system_programs
