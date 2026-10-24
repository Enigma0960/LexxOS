#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::system_programs {

/**
 * @lexxos-command-doc run
 * @source src/lexxos/apps/core/run_program.cpp
 * @description
 * Запускает программу через экспериментальный `ProgramSupervisor` task path. Foreground-запуск ждёт завершения
 * дочерней task, а background-запуск возвращает shell сразу и показывает process id для `jobs`/`stop`.
 *
 * @parameters
 * - `--bg` / `-b` — запустить программу как background job, если её `ProgramRuntimeProfile` разрешает это.
 * - `--console` / `-c` — оставить stdout/stderr background job в текущей консоли.
 * - `--quiet` / `-q` — перенаправить stdout/stderr background job в `NullOutputStream`; это поведение по умолчанию.
 * - `<program> [args...]` — имя программы и её аргументы; передаются дочернему `ProgramStartRequest`.
 *
 * @examples
 * ```text
 * lexx:/> run board
 * lexx:/> run edit --help
 * lexx:/> run sd-bench 1
 * lexx:/> run --bg sd-bench --delay-ms 10 1024
 * ```
 *
 * @author
 * Обычный shell-dispatch пока остаётся основным путём. `run` нужен для проверки stack high-water mark, cooperative
 * cancellation и будущей модели параллельных задач до включения supervisor по умолчанию.
 * @end
 */
class RunProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::system_programs
