#include <lexxos/apps/fs/copy_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using CopyBuffer = std::array<char, 128>;

	constexpr std::uint32_t copyWorkspaceBytes =
	    static_cast<std::uint32_t>((lexxos::vfs::maxPathLength * 2U) + sizeof(CopyBuffer) + alignof(CopyBuffer));

	constexpr lexxos::program::ProgramArgumentSpec copyProgramArguments[] = {
	    {
         .name = "source",
         .valueName = "source",
         .help = "readable VFS source file",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	    {
         .name = "target",
         .valueName = "target",
         .help = "writable VFS target file",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("cp: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("cp: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& sourcePath, PathBuffer*& targetPath, CopyBuffer*& copyBuffer) {
		return allocateWorkspaceObject(context, sourcePath) && allocateWorkspaceObject(context, targetPath) && allocateWorkspaceObject(context, copyBuffer);
	}

	bool copyFileToFile(
	    lexxos::os::AppContext& context,
	    std::string_view sourcePath,
	    lexxos::vfs::FileHandle& source,
	    std::string_view targetPath,
	    lexxos::vfs::FileHandle& target,
	    CopyBuffer& buffer,
	    std::uint64_t& bytesCopied) {
		bytesCopied = 0;
		while (true) {
			std::size_t bytesRead = 0;
			auto const readStatus = source.read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				context.write("cp: ");
				context.write(sourcePath);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(readStatus));
				return false;
			}
			if (bytesRead == 0) {
				break;
			}

			auto const writeStatus = target.write(std::string_view{buffer.data(), bytesRead});
			if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
				context.write("cp: ");
				context.write(targetPath);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(writeStatus));
				return false;
			}
			bytesCopied += bytesRead;
		}
		return true;
	}

} // namespace

ProgramDescriptor CopyProgram::descriptor() {
	return {
	    .name = "cp",
	    .help = "copy readable VFS file to writable VFS file",
	    .description = "Copies data from a readable VFS file to a writable VFS file using FileHandle read/write operations. Targets may be /tmp or a writable FAT mount.",
	    .usage = "cp <source> <target>",
	    .arguments = argumentList(copyProgramArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = copyWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int CopyProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* sourcePath = nullptr;
	PathBuffer* targetPath = nullptr;
	CopyBuffer* copyBuffer = nullptr;
	if (!allocateWorkspace(context, sourcePath, targetPath, copyBuffer)) {
		return 1;
	}

	// Resolve both paths and reject an in-place copy.
	if (!resolvePath(context, arguments.positional("source"), *sourcePath)) {
		return 1;
	}

	if (!resolvePath(context, arguments.positional("target"), *targetPath)) {
		return 1;
	}

	if (std::string_view{sourcePath->data()} == std::string_view{targetPath->data()}) {
		context.write("cp: source and target are the same: ");
		context.writeLine(sourcePath->data());
		return 1;
	}

	// Open the source for reading and recreate the destination.
	lexxos::vfs::FileHandle source;
	auto const sourceOpenStatus = lexxos::vfs::open(sourcePath->data(), lexxos::vfs::OpenMode::Read, source);
	if (sourceOpenStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("cp: ");
		context.write(sourcePath->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(sourceOpenStatus));
		return 1;
	}

	lexxos::vfs::FileHandle target;
	auto const targetOpenStatus = lexxos::vfs::open(targetPath->data(), lexxos::vfs::OpenMode::CreateTruncateWrite, target);
	if (targetOpenStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("cp: ");
		context.write(targetPath->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(targetOpenStatus));
		source.close();
		return 1;
	}

	// Transfer all bytes, then close both handles before verification.
	std::uint64_t bytesCopied = 0;
	bool const copied = copyFileToFile(context, sourcePath->data(), source, targetPath->data(), target, *copyBuffer, bytesCopied);
	target.close();
	source.close();
	if (!copied) {
		return 1;
	}

	// Verify regular destinations; /dev/null intentionally has no stored size.
	if (std::string_view{targetPath->data()} != std::string_view{"/dev/null"}) {
		lexxos::vfs::Stat targetStat{};
		if (!lexxos::vfs::stat(targetPath->data(), targetStat)) {
			context.write("cp: ");
			context.write(targetPath->data());
			context.writeLine(": target verification failed: not found after copy");
			return 1;
		}
		if (targetStat.type != lexxos::vfs::NodeType::RegularFile) {
			context.write("cp: ");
			context.write(targetPath->data());
			context.writeLine(": target verification failed: not a regular file");
			return 1;
		}
		if (targetStat.sizeBytes != bytesCopied) {
			context.write("cp: ");
			context.write(targetPath->data());
			context.write(": target verification failed: expected ");
			writeUnsigned64(context, bytesCopied);
			context.write(" bytes, got ");
			writeUnsigned64(context, targetStat.sizeBytes);
			context.writeLine(" bytes");
			return 1;
		}
	}

	return 0;
}

} // namespace lexxos::apps::filesystem_programs
