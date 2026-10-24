#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::lexx_programs {

/**
 * @lexxos-command-doc lexx-info
 * @source src/lexxos/apps/lexx/lexx_info_program.cpp
 * @description
 * Показывает формат, ABI, память и манифест внешней программы без исполнения кода.
 *
 * @parameters
 * - --json: вывести только содержимое манифеста.
 * - file.lexx: путь к исполняемому файлу.
 *
 * @examples
 * lexx-info /mnt/sd/apps/hello.lexx
 * lexx-info --json /mnt/sd/apps/hello.lexx
 * @end
 */
class LexxInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();

private:
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::lexx_programs
