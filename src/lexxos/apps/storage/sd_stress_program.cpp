#include <lexxos/apps/storage/sd_stress_program.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::storage_programs {

using detail::elapsedMs;
using detail::fillPattern;
using detail::makeStressPath;
using detail::parseOptionalCount;
using detail::sdStressDefaultFiles;
using detail::sdStressDefaultPasses;
using detail::sdStressMaxFiles;
using detail::sdStressMaxPasses;
using detail::storageBlockSize;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using BlockBuffer = std::array<char, storageBlockSize>;

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr std::uint32_t sdStressWorkspaceBytes = static_cast<std::uint32_t>(
	    sizeof(PathBuffer) + (sizeof(BlockBuffer) * 2U) + alignof(PathBuffer) + (alignof(BlockBuffer) * 2U));

	constexpr ProgramArgumentSpec sdStressArguments[] = {
	    {
         .name = "passes",
         .valueName = "passes",
         .help = "optional pass count, default: 3, max: 20",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "files",
         .valueName = "files",
         .help = "optional file count per pass, default: 4, max: 8",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("sd-stress: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("sd-stress: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(
	    lexxos::os::AppContext& context,
	    PathBuffer*& path,
	    BlockBuffer*& writeBuffer,
	    BlockBuffer*& readBuffer) {
		return allocateWorkspaceObject(context, path) && allocateWorkspaceObject(context, writeBuffer) && allocateWorkspaceObject(context, readBuffer);
	}

	bool cancellationRequested(lexxos::os::AppContext& context) {
		if (!context.cancellationRequested()) {
			return false;
		}

		context.writeLine("sd-stress: cancelled");
		return true;
	}

} // namespace

ProgramDescriptor SdStressProgram::descriptor() {
	return {
	    .name = "sd-stress",
	    .help = "run repeated FAT create/read/delete test",
	    .description = "Creates several temporary files on /mnt/sd, verifies their contents and removes them repeatedly.",
	    .usage = "sd-stress [passes] [files]",
	    .arguments = argumentList(sdStressArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = sdStressWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int SdStressProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::uint32_t passes = 0;
	if (!parseOptionalCount(context, arguments.positional("passes"), sdStressDefaultPasses, sdStressMaxPasses, "sd-stress passes", passes)) {
		return 1;
	}

	std::uint32_t files = 0;
	if (!parseOptionalCount(context, arguments.positional("files"), sdStressDefaultFiles, sdStressMaxFiles, "sd-stress files", files)) {
		return 1;
	}

	PathBuffer* path = nullptr;
	BlockBuffer* writeBuffer = nullptr;
	BlockBuffer* readBuffer = nullptr;
	if (!allocateWorkspace(context, path, writeBuffer, readBuffer)) {
		return 1;
	}

	std::uint32_t const startedMs = uptimeMs(context);
	std::uint32_t createdFiles = 0;

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		if (cancellationRequested(context)) {
			return 130;
		}

		context.write("sd-stress: pass ");
		writeUnsigned(context, pass + 1);
		context.write("/");
		writeUnsigned(context, passes);
		context.writeLine("");

		for (std::uint32_t fileIndex = 0; fileIndex < files; ++fileIndex) {
			if (cancellationRequested(context)) {
				return 130;
			}

			if (!makeStressPath(pass, fileIndex, *path)) {
				context.writeLine("sd-stress: path too long");
				return 1;
			}

			std::uint32_t const seed = 31U + pass * 7U + fileIndex;
			fillPattern(*writeBuffer, seed, 0);

			lexxos::vfs::FileHandle output;
			auto openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::CreateTruncateWrite, output);
			if (openStatus != lexxos::vfs::OpenStatus::Ok) {
				context.write("sd-stress: open write failed: ");
				context.write(path->data());
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(openStatus));
				return 1;
			}
			auto const writeStatus = output.write(std::string_view{writeBuffer->data(), writeBuffer->size()});
			output.close();
			if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
				context.write("sd-stress: write failed: ");
				context.writeLine(lexxos::vfs::toString(writeStatus));
				return 1;
			}

			lexxos::vfs::FileHandle input;
			openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, input);
			if (openStatus != lexxos::vfs::OpenStatus::Ok) {
				context.write("sd-stress: open read failed: ");
				context.writeLine(lexxos::vfs::toString(openStatus));
				return 1;
			}
			std::size_t bytesRead = 0;
			auto const readStatus = input.read(readBuffer->data(), readBuffer->size(), bytesRead);
			input.close();
			if (readStatus != lexxos::vfs::ReadStatus::Ok || bytesRead != readBuffer->size()) {
				context.write("sd-stress: read failed: ");
				if (readStatus == lexxos::vfs::ReadStatus::Ok) {
					context.writeLine("short read");
				} else {
					context.writeLine(lexxos::vfs::toString(readStatus));
				}
				return 1;
			}
			if (std::memcmp(writeBuffer->data(), readBuffer->data(), readBuffer->size()) != 0) {
				context.write("sd-stress: verify failed: ");
				context.writeLine(path->data());
				return 1;
			}

			auto const removeStatus = lexxos::vfs::remove(path->data());
			if (removeStatus != lexxos::vfs::RemoveStatus::Ok) {
				context.write("sd-stress: remove failed: ");
				context.writeLine(lexxos::vfs::toString(removeStatus));
				return 1;
			}
			++createdFiles;
		}

		auto const syncStatus = lexxos::vfs::sync();
		if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
			context.write("sd-stress: sync failed: ");
			context.writeLine(lexxos::vfs::toString(syncStatus));
			return 1;
		}
	}

	std::uint32_t const totalMs = elapsedMs(startedMs, uptimeMs(context));
	context.write("sd-stress: ok, files=");
	writeUnsigned(context, createdFiles);
	context.write(", elapsed=");
	writeUnsigned(context, totalMs);
	context.writeLine(" ms");
	return 0;
}

} // namespace lexxos::apps::storage_programs
