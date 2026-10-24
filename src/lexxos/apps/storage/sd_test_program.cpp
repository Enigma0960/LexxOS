#include <lexxos/apps/storage/sd_test_program.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::storage_programs {

using detail::sdTestDefaultPath;
using detail::sdTestPayload;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using ReadBuffer = std::array<char, 96>;

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr std::uint32_t sdTestWorkspaceBytes =
	    static_cast<std::uint32_t>(sizeof(PathBuffer) + sizeof(ReadBuffer) + alignof(ReadBuffer));

	constexpr ProgramArgumentSpec sdTestArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional writable FAT test file path, default: /mnt/sd/LEXTEST.TXT",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("sd-test: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("sd-test: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& path, ReadBuffer*& readBuffer) {
		return allocateWorkspaceObject(context, path) && allocateWorkspaceObject(context, readBuffer);
	}

} // namespace

ProgramDescriptor SdTestProgram::descriptor() {
	return {
	    .name = "sd-test",
	    .help = "run SD FAT read/write smoke test",
	    .description = "Writes a small file to /mnt/sd, reads it back, compares contents and removes the test file.",
	    .usage = "sd-test [path]",
	    .arguments = argumentList(sdTestArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = sdTestWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int SdTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	PathBuffer* path = nullptr;
	ReadBuffer* readBuffer = nullptr;
	if (!allocateWorkspace(context, path, readBuffer)) {
		return 1;
	}

	if (!resolvePath(context, arguments.positionalOr("path", sdTestDefaultPath), *path)) {
		return 1;
	}

	context.write("sd-test: writing ");
	context.writeLine(path->data());
	auto const writeStatus = lexxos::vfs::writeText(path->data(), sdTestPayload, lexxos::vfs::WriteMode::CreateTruncate);
	if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
		context.write("sd-test: write failed: ");
		context.writeLine(lexxos::vfs::toString(writeStatus));
		return 1;
	}

	auto const syncStatus = lexxos::vfs::sync();
	if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
		context.write("sd-test: sync failed: ");
		context.writeLine(lexxos::vfs::toString(syncStatus));
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("sd-test: read open failed: ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	std::size_t bytesRead = 0;
	auto const readStatus = file.read(readBuffer->data(), readBuffer->size(), bytesRead);
	file.close();
	if (readStatus != lexxos::vfs::ReadStatus::Ok) {
		context.write("sd-test: read failed: ");
		context.writeLine(lexxos::vfs::toString(readStatus));
		return 1;
	}

	if (bytesRead != sdTestPayload.size() || std::memcmp(readBuffer->data(), sdTestPayload.data(), sdTestPayload.size()) != 0) {
		context.write("sd-test: verify failed, bytes=");
		writeUnsigned64(context, bytesRead);
		context.writeLine("");
		return 1;
	}

	context.write("sd-test: readback ok, bytes=");
	writeUnsigned64(context, bytesRead);
	context.writeLine("");

	auto const removeStatus = lexxos::vfs::remove(path->data());
	if (removeStatus != lexxos::vfs::RemoveStatus::Ok) {
		context.write("sd-test: remove failed: ");
		context.writeLine(lexxos::vfs::toString(removeStatus));
		return 1;
	}

	context.writeLine("sd-test: removed test file");
	return 0;
}

} // namespace lexxos::apps::storage_programs
