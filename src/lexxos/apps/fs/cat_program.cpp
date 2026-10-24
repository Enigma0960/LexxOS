#include <lexxos/apps/fs/cat_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using CatBuffer = std::array<char, 96>;

	constexpr std::uint32_t catWorkspaceBytes =
	    static_cast<std::uint32_t>(lexxos::vfs::maxPathLength + sizeof(CatBuffer) + alignof(CatBuffer));
#if defined(LEXXOS_KERNEL_ESPIDF) || defined(LEXXOS_PLATFORM_ESPIDF)
	constexpr std::uint32_t catStackBytes = 6144;
#else
	constexpr std::uint32_t catStackBytes = 4096;
#endif

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("cat: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("cat: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& path, CatBuffer*& buffer) {
		return allocateWorkspaceObject(context, path) && allocateWorkspaceObject(context, buffer);
	}

	bool copyInputToOutput(lexxos::os::AppContext& context, CatBuffer& buffer) {
		while (true) {
			std::size_t bytesRead = 0;
			auto const readStatus = context.standardInput().read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				context.writeLine("cat: stdin: read failed");
				return false;
			}
			if (bytesRead == 0) {
				break;
			}
			context.write(std::string_view{buffer.data(), bytesRead});
		}
		return true;
	}

	bool copyFileToOutput(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    lexxos::vfs::FileHandle& file,
	    CatBuffer& buffer) {
		while (true) {
			std::size_t bytesRead = 0;
			auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				context.write("cat: ");
				context.write(path);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(readStatus));
				return false;
			}
			if (bytesRead == 0) {
				break;
			}
			context.write(std::string_view{buffer.data(), bytesRead});
		}
		return true;
	}

} // namespace

ProgramDescriptor CatProgram::descriptor() {
	return {
	    .name = "cat",
	    .help = "print readable VFS file or stdin",
	    .description = "Prints one or more readable VFS files. With no path it reads from standard input, so shell redirection like cat < /tmp/file works.",
	    .usage = "cat [path ...]",
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = catStackBytes,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = catWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .allowExtraArguments = true,
	};
}

int CatProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	PathBuffer* path = nullptr;
	CatBuffer* buffer = nullptr;
	if (!allocateWorkspace(context, path, buffer)) {
		return 1;
	}

	// With no file arguments, forward standard input unchanged.
	if (invocation.argc <= 1) {
		return copyInputToOutput(context, *buffer) ? 0 : 1;
	}

	// Resolve and stream each file in argument order.
	for (int index = 1; index < invocation.argc; ++index) {
		if (!resolvePath(context, invocation.argv[index], *path)) {
			return 1;
		}

		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			context.write("cat: ");
			context.write(path->data());
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(openStatus));
			return 1;
		}
		if (!copyFileToOutput(context, path->data(), file, *buffer)) {
			return 1;
		}
	}

	return 0;
}

} // namespace lexxos::apps::filesystem_programs
