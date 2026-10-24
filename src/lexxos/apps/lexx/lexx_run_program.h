#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::lexx_programs {

/**
 * @lexxos-command-doc lexx-run
 * @source src/lexxos/apps/lexx/lexx_run_program.cpp
 * @description
 * Запускает внешний исполняемый файл `.lexx` из VFS/SD через экспериментальный RV32I runtime. Команда валидирует
 * заголовок LEXX, выделяет ограниченную guest-память, передает guest-программе `argc/argv` и выполняет системные
 * вызовы текстовой консоли, клавиатуры и read-only файлового доступа.
 *
 * @parameters
 * - `--trace` - печатать PC и инструкцию перед каждым шагом RV32I-интерпретатора.
 * - `--max-steps <count>` - ограничить число исполненных инструкций; `0` означает запуск без лимита.
 * - `<file.lexx> [args...]` - путь к внешней программе и аргументы, которые guest увидит как `argv`.
 * - `--` - завершить разбор опций `lexx-run`, если путь к файлу начинается с дефиса.
 *
 * @examples
 * ```text
 * lexx:/> lexx-run /mnt/sd/ext/hello.lexx alpha beta
 * lexx:/> lexx-run --trace /mnt/sd/ext/hello.lexx
 * lexx:/> lexx-run --max-steps 500000 /mnt/sd/ext/hello.lexx
 * ```
 *
 * @author
 * Прямой запуск `/mnt/sd/ext/app.lexx` из shell остается основным пользовательским путем; `lexx-run` нужен для
 * явной диагностики runtime и лимитов.
 * @end
 */
class LexxRunProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();

private:
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::lexx_programs
