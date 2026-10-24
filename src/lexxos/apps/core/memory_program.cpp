#include <lexxos/apps/core/memory_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor MemoryProgram::descriptor() {
	return {.name = "mem", .help = "show kernel memory statistics", .handler = run, .legacy = false, .enabled = true};
}

int MemoryProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const memory = lexxos::kernel::memorySnapshot();
	if (!memory.available) {
		context.writeLine("memory statistics are not available on this backend");
		return 0;
	}
	context.write("free heap: ");
	writeUnsigned(context, memory.freeHeapBytes);
	context.writeLine(" bytes");
	context.write("minimum ever free heap: ");
	writeUnsigned(context, memory.minimumEverFreeHeapBytes);
	context.writeLine(" bytes");
	return 0;
}

} // namespace lexxos::apps::system_programs
