#include <lexxos/apps/fs/rename_program.h>

#include <lexxos/apps/fs/move_program.h>

namespace lexxos::apps::filesystem_programs {

lexxos::program::ProgramDescriptor RenameProgram::descriptor() {
	// Reuse the complete mv descriptor, including the foreground task runtime
	// and its workspace quota: the handler allocates paths from that arena.
	auto descriptor = MoveProgram::descriptor();
	descriptor.name = "rename";
	descriptor.description = "Alias for mv. Renames or moves a file or directory inside the same writable filesystem.";
	descriptor.usage = "rename <source> <target>";
	return descriptor;
}

} // namespace lexxos::apps::filesystem_programs
