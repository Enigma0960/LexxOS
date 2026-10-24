#include <lexxos/apps/editor/edit_program.h>

#include <array>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/editor/detail/editor_controller.h>
#include <lexxos/apps/editor/detail/editor_file_service.h>
#include <lexxos/apps/editor/detail/text_editor_types.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::text_editor_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramDescriptor;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr ProgramArgumentSpec editArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "VFS text file path",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = true,
	     },
	};

	detail::EditorSession* allocateEditorSession(lexxos::os::AppContext& context) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("edit: program memory arena is unavailable");
			return nullptr;
		}

		auto* const memory = arena->allocate(sizeof(detail::EditorSession), alignof(detail::EditorSession));
		if (memory == nullptr) {
			context.writeLine("edit: not enough program workspace memory");
			return nullptr;
		}

		return new (memory) detail::EditorSession{};
	}

} // namespace

ProgramDescriptor EditProgram::descriptor() {
	return {
	    .name = "edit",
	    .help = "edit a VFS text file",
	    .description = "Starts a small screen-oriented text editor for ASCII files. It keeps the edited file in a fixed RAM buffer and writes it back after Ctrl+S.",
	    .usage = "edit <path>",
	    .arguments = argumentList(editArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::ExternalPreferred,
	                .externalMemoryQuotaBytes = 128 * 1024,
	                .internalMemoryQuotaBytes = sizeof(detail::EditorSession),
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int EditProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("path"), path)) {
		return 1;
	}

	auto* const session = allocateEditorSession(context);
	if (session == nullptr) {
		return 1;
	}

	bool createdNewFile = false;
	if (!detail::EditorFileService::load(context, path.data(), session->buffer, createdNewFile)) {
		return 1;
	}
	(void) createdNewFile;

	return detail::EditorController::run(context, path.data(), session->buffer, session->state);
}

} // namespace lexxos::apps::text_editor_programs
