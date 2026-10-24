#include <lexxos/apps/fs/detail/text_file_writer.h>

#include <array>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/arguments.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_memory_arena.h>

namespace lexxos::apps::filesystem_programs::detail {
namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using TextFileWriterBuffer = std::array<char, textFileWriterStreamBufferBytes>;

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, std::string_view commandName, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.write(commandName);
			context.writeLine(": program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.write(commandName);
			context.writeLine(": not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(
	    lexxos::os::AppContext& context,
	    std::string_view commandName,
	    PathBuffer*& path,
	    TextFileWriterBuffer*& buffer) {
		return allocateWorkspaceObject(context, commandName, path)
		    && allocateWorkspaceObject(context, commandName, buffer);
	}

	bool copyInputToFile(
	    lexxos::os::AppContext& context,
	    std::string_view commandName,
	    std::string_view path,
	    lexxos::vfs::FileHandle& file,
	    TextFileWriterBuffer& buffer) {
		while (true) {
			std::size_t bytesRead = 0;
			auto const readStatus = context.standardInput().read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				context.write(commandName);
				context.write(": ");
				context.write(path);
				context.writeLine(": stdin: read failed");
				return false;
			}
			if (bytesRead == 0) {
				break;
			}

			auto const writeStatus = file.write(std::string_view{buffer.data(), bytesRead});
			if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
				context.write(commandName);
				context.write(": ");
				context.write(path);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(writeStatus));
				return false;
			}
		}
		return true;
	}

	bool writeFileChunk(
	    lexxos::os::AppContext& context,
	    std::string_view commandName,
	    std::string_view path,
	    lexxos::vfs::FileHandle& file,
	    std::string_view text) {
		auto const status = file.write(text);
		if (status == lexxos::vfs::WriteStatus::Ok) {
			return true;
		}

		context.write(commandName);
		context.write(": ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return false;
	}

} // namespace

int runTextFileWriter(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::vfs::OpenMode mode,
    std::string_view commandName) {
	PathBuffer* path = nullptr;
	TextFileWriterBuffer* buffer = nullptr;
	if (!allocateWorkspace(context, commandName, path, buffer)) {
		return 1;
	}

	// Resolve and open the destination using the mode selected by the command.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path->data(), mode, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write(commandName);
		context.write(": ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	// With no text arguments, stream standard input into the file.
	constexpr int firstTextArgument = 2;
	if (invocation.argc <= firstTextArgument) {
		return copyInputToFile(context, commandName, path->data(), file, *buffer) ? 0 : 1;
	}

	// Join command-line text arguments with spaces and terminate the line.
	if (!writeFileChunk(context, commandName, path->data(), file, invocation.argv[firstTextArgument])) {
		return 1;
	}
	for (int index = firstTextArgument + 1; index < invocation.argc; ++index) {
		if (!writeFileChunk(context, commandName, path->data(), file, " ")
		    || !writeFileChunk(context, commandName, path->data(), file, invocation.argv[index])) {
			return 1;
		}
	}

	return writeFileChunk(context, commandName, path->data(), file, "\n") ? 0 : 1;
}

} // namespace lexxos::apps::filesystem_programs::detail
