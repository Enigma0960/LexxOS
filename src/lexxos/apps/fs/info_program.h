#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::filesystem_programs {

/**
 * @lexxos-command-doc info
 * @source src/lexxos/apps/fs/info_program.cpp
 * @description
 * Показывает сведения об одном файле или каталоге VFS, включая доступные
 * FAT-атрибуты и дату изменения. Для .lexx дополнительно проверяет заголовок
 * и CRC32 встроенного манифеста и выводит его поля.
 *
 * @parameters
 * - path: путь к одному файлу или каталогу; шаблоны * и ? не поддерживаются.
 *
 * @examples
 * info /mnt/sd/apps/fileman.lexx
 * info /mnt/sd/docs
 * info /proc/tasks
 * @end
 */
class InfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::filesystem_programs
