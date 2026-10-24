#include <lexxos/apps/fs/fs_smoke_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/fs_smoke_types.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using CopyBuffer = std::array<char, 64>;

	constexpr std::uint32_t fsSmokeWorkspaceBytes =
	    static_cast<std::uint32_t>((lexxos::vfs::maxPathLength * 5U) + sizeof(detail::SmokeReadBuffer) + sizeof(CopyBuffer) + alignof(detail::SmokeReadBuffer) + alignof(CopyBuffer));

	constexpr lexxos::program::ProgramArgumentSpec fsSmokeArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional VFS path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("fs-smoke: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("fs-smoke: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context,
	    PathBuffer*& base,
	    PathBuffer*& directory,
	    PathBuffer*& file,
	    PathBuffer*& copy,
	    PathBuffer*& moved,
	    detail::SmokeReadBuffer*& readBuffer,
	    CopyBuffer*& copyBuffer) {
		return allocateWorkspaceObject(context, base) && allocateWorkspaceObject(context, directory) && allocateWorkspaceObject(context, file) && allocateWorkspaceObject(context, copy) && allocateWorkspaceObject(context, moved) && allocateWorkspaceObject(context, readBuffer) && allocateWorkspaceObject(context, copyBuffer);
	}

	void smokeReadWriter(std::string_view text, void* userData) {
		auto& buffer = *static_cast<detail::SmokeReadBuffer*>(userData);
		for (char const character : text) {
			if (buffer.size >= buffer.data.size()) {
				buffer.overflow = true;
				return;
			}
			buffer.data[buffer.size++] = character;
		}
	}

	bool smokeAppendPath(std::string_view base, std::string_view name, std::array<char, lexxos::vfs::maxPathLength>& output) {
		output = {};
		std::size_t size = 0;
		auto append = [&](char value) -> bool {
			if (size + 1 >= output.size()) {
				return false;
			}
			output[size++] = value;
			output[size] = '\0';
			return true;
		};
		for (char const character : base) {
			if (!append(character)) {
				return false;
			}
		}
		if (size == 0) {
			return false;
		}
		if (output[size - 1] != '/') {
			if (!append('/')) {
				return false;
			}
		}
		for (char const character : name) {
			if (!append(character)) {
				return false;
			}
		}
		return true;
	}

	bool smokeExpect(lexxos::os::AppContext& context, bool ok, std::string_view step) {
		context.write("  ");
		context.write(ok ? "ok   " : "FAIL ");
		context.writeLine(step);
		return ok;
	}

	bool smokeCopyFile(std::string_view sourcePath, std::string_view targetPath, CopyBuffer& buffer) {
		lexxos::vfs::FileHandle source;
		auto status = lexxos::vfs::open(sourcePath, lexxos::vfs::OpenMode::Read, source);
		if (status != lexxos::vfs::OpenStatus::Ok) {
			return false;
		}

		lexxos::vfs::FileHandle target;
		status = lexxos::vfs::open(targetPath, lexxos::vfs::OpenMode::CreateTruncateWrite, target);
		if (status != lexxos::vfs::OpenStatus::Ok) {
			source.close();
			return false;
		}

		while (true) {
			std::size_t bytesRead = 0;
			auto const readStatus = source.read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				target.close();
				source.close();
				return false;
			}
			if (bytesRead == 0) {
				break;
			}
			auto const writeStatus = target.write(std::string_view{buffer.data(), bytesRead});
			if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
				target.close();
				source.close();
				return false;
			}
		}
		target.close();
		source.close();
		return true;
	}

	void smokeCleanup(
	    PathBuffer const& directory,
	    PathBuffer const& file,
	    PathBuffer const& copy,
	    PathBuffer const& moved) {
		lexxos::vfs::remove(file.data());
		lexxos::vfs::remove(copy.data());
		lexxos::vfs::remove(moved.data());
		lexxos::vfs::removeDirectory(directory.data());
	}

} // namespace

ProgramDescriptor FsSmokeProgram::descriptor() {
	return {
	    .name = "fs-smoke",
	    .help = "run VFS writable filesystem smoke test",
	    .description = "Creates a temporary test directory and verifies long names, write, append, read, copy, rename, remove, rmdir and sync on a writable VFS mount.",
	    .usage = "fs-smoke [path]",
	    .arguments = argumentList(fsSmokeArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = fsSmokeWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int FsSmokeProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* base = nullptr;
	PathBuffer* directory = nullptr;
	PathBuffer* file = nullptr;
	PathBuffer* copy = nullptr;
	PathBuffer* moved = nullptr;
	detail::SmokeReadBuffer* readBuffer = nullptr;
	CopyBuffer* copyBuffer = nullptr;
	if (!allocateWorkspace(context, base, directory, file, copy, moved, readBuffer, copyBuffer)) {
		return 1;
	}

	// Resolve and validate the writable test root.
	if (!resolvePath(context, arguments.positionalOr("path", "/mnt/sd"), *base)) {
		return 1;
	}

	lexxos::vfs::Stat baseStat{};
	if (!lexxos::vfs::stat(base->data(), baseStat) || baseStat.type != lexxos::vfs::NodeType::Directory) {
		context.write("fs-smoke: not a writable directory: ");
		context.writeLine(base->data());
		return 1;
	}

	// Build deterministic long-name paths and clear leftovers from previous runs.
	if (!smokeAppendPath(base->data(), "fatfs-smoke", *directory)
	    || !smokeAppendPath(directory->data(), "hello-long-name.txt", *file)
	    || !smokeAppendPath(directory->data(), "copied-long-name.txt", *copy)
	    || !smokeAppendPath(directory->data(), "moved-long-name.txt", *moved)) {
		context.writeLine("fs-smoke: generated path is too long");
		return 1;
	}

	context.write("fs-smoke: base ");
	context.writeLine(base->data());
	smokeCleanup(*directory, *file, *copy, *moved);

	// Exercise create, write, append and content verification.
	bool ok = true;
	auto const mkdirStatus = lexxos::vfs::createDirectory(directory->data());
	ok = smokeExpect(context, mkdirStatus == lexxos::vfs::WriteStatus::Ok, "mkdir long directory") && ok;

	auto const writeStatus = lexxos::vfs::writeText(file->data(), "hello\n", lexxos::vfs::WriteMode::CreateTruncate);
	ok = smokeExpect(context, writeStatus == lexxos::vfs::WriteStatus::Ok, "write long file") && ok;

	auto const appendStatus = lexxos::vfs::writeText(file->data(), "world\n", lexxos::vfs::WriteMode::Append);
	ok = smokeExpect(context, appendStatus == lexxos::vfs::WriteStatus::Ok, "append long file") && ok;

	auto const readStatus = lexxos::vfs::readText(file->data(), smokeReadWriter, readBuffer);
	std::string_view const expected{"hello\nworld\n"};
	bool const readOk = readStatus == lexxos::vfs::ReadStatus::Ok && !readBuffer->overflow && readBuffer->size == expected.size() && std::string_view{readBuffer->data.data(), readBuffer->size} == expected;
	ok = smokeExpect(context, readOk, "read and verify content") && ok;

	// Exercise copy, rename, metadata and cleanup operations.
	ok = smokeExpect(context, smokeCopyFile(file->data(), copy->data(), *copyBuffer), "copy file") && ok;

	auto const renameStatus = lexxos::vfs::rename(copy->data(), moved->data());
	ok = smokeExpect(context, renameStatus == lexxos::vfs::RenameStatus::Ok, "rename copied file") && ok;

	lexxos::vfs::Stat movedStat{};
	bool const statOk = lexxos::vfs::stat(moved->data(), movedStat) && movedStat.type == lexxos::vfs::NodeType::RegularFile && movedStat.sizeBytes == expected.size();
	ok = smokeExpect(context, statOk, "stat renamed file") && ok;

	auto const removeOriginalStatus = lexxos::vfs::remove(file->data());
	ok = smokeExpect(context, removeOriginalStatus == lexxos::vfs::RemoveStatus::Ok, "remove original file") && ok;

	auto const removeMovedStatus = lexxos::vfs::remove(moved->data());
	ok = smokeExpect(context, removeMovedStatus == lexxos::vfs::RemoveStatus::Ok, "remove renamed file") && ok;

	auto const rmdirStatus = lexxos::vfs::removeDirectory(directory->data());
	ok = smokeExpect(context, rmdirStatus == lexxos::vfs::RemoveStatus::Ok, "remove smoke directory") && ok;

	// Flush the filesystem and report the aggregate result.
	auto const syncStatus = lexxos::vfs::sync();
	ok = smokeExpect(context, syncStatus == lexxos::vfs::SyncStatus::Ok, "sync") && ok;

	context.writeLine(ok ? "fs-smoke: ok" : "fs-smoke: failed");
	return ok ? 0 : 1;
}

} // namespace lexxos::apps::filesystem_programs
