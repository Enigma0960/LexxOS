#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::system_programs {

/**
 * @lexxos-command-doc stop
 * @source src/lexxos/apps/core/stop_program.cpp
 * @description
 * Запрашивает cooperative cancellation для процесса, созданного `ProgramSupervisor`. Команда не убивает task
 * принудительно: процесс должен сам проверить `CancellationToken` и завершиться.
 *
 * @parameters
 * - `<id>` — обязательный id supervised process из вывода `jobs`.
 *
 * @examples
 * ```text
 * lexx:/> run --bg sd-bench --delay-ms 10 1024
 * lexx:/> jobs
 * lexx:/> stop 1
 * ```
 *
 * @author
 * Для LexxOS 0.0.7 это намеренно мягкая остановка. Штатный kill через `vTaskDelete(handle)` не используется, чтобы не
 * оставлять занятые ресурсы, файлы и будущие PSRAM arena allocations в неизвестном состоянии.
 * @end
 */
class StopProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::system_programs
