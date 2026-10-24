#include <lexxos/apps/storage/sd_bench_program.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/storage_program_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::storage_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

using detail::elapsedMs;
using detail::fillPattern;
using detail::parseOptionalCount;
using detail::sdBenchDefaultBlocks;
using detail::sdBenchDefaultPath;
using detail::sdBenchMaxBlocks;
using detail::storageBlockSize;
using detail::verifyPattern;
using detail::writeThroughput;

namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr ProgramArgumentSpec sdBenchArguments[] = {
	    {
         .name = "blocks",
         .valueName = "blocks",
         .help = "optional 512-byte block count, default: 128, max: 1024",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional FAT benchmark file path, default: /mnt/sd/LEXBENCH.TXT",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = false,
	     },
	    {
         .name = "delay-ms",
         .shortName = 'd',
         .valueName = "ms",
         .help = "optional delay after each 512-byte block, default: 0, max: 1000",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	};

	bool allocateWorkspace(
	    lexxos::os::AppContext& context,
	    std::array<char, lexxos::vfs::maxPathLength>*& path,
	    std::array<char, storageBlockSize>*& ioBuffer) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("sd-bench: program memory arena is unavailable");
			return false;
		}

		auto* const pathMemory = arena->allocate(sizeof(std::array<char, lexxos::vfs::maxPathLength>), alignof(std::array<char, lexxos::vfs::maxPathLength>));
		auto* const ioMemory = arena->allocate(sizeof(std::array<char, storageBlockSize>), alignof(std::array<char, storageBlockSize>));
		if (pathMemory == nullptr || ioMemory == nullptr) {
			context.writeLine("sd-bench: not enough program workspace memory");
			return false;
		}

		path = new (pathMemory) std::array<char, lexxos::vfs::maxPathLength>{};
		ioBuffer = new (ioMemory) std::array<char, storageBlockSize>{};
		return true;
	}

	bool cancellationRequested(lexxos::os::AppContext& context) {
		if (!context.cancellationRequested()) {
			return false;
		}

		context.writeLine("sd-bench: cancelled");
		return true;
	}

	bool parseDelayMs(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::uint32_t& delayMs) {
		auto const value = arguments.valueOr("delay-ms", "0");
		if (!parseUnsigned32(value, delayMs) || delayMs > 1000U) {
			context.writeLine("sd-bench delay-ms: expected 0..1000");
			return false;
		}

		return true;
	}

	bool waitAfterBlock(lexxos::os::AppContext& context, std::uint32_t delayMs) {
		std::uint32_t remaining = delayMs;
		while (remaining > 0) {
			if (cancellationRequested(context)) {
				return true;
			}
			auto const slice = remaining > 25U ? 25U : remaining;
			sleepMs(context, slice);
			remaining -= slice;
		}

		return cancellationRequested(context);
	}

} // namespace

ProgramDescriptor SdBenchProgram::descriptor() {
	return {
	    .name = "sd-bench",
	    .help = "benchmark SD raw read and FAT read/write",
	    .description = "Reads raw SD blocks, writes a temporary FAT file, reads it back and verifies a generated pattern.",
	    .usage = "sd-bench [--delay-ms ms] [blocks] [path]",
	    .arguments = argumentList(sdBenchArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = lexxos::vfs::maxPathLength + storageBlockSize,
	                .cancellable = true,
	                .allowBackground = true,
	                },
	};
}

int SdBenchProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::uint32_t blocks = 0;
	if (!parseOptionalCount(context, arguments.positional("blocks"), sdBenchDefaultBlocks, sdBenchMaxBlocks, "sd-bench blocks", blocks)) {
		return 1;
	}

	std::array<char, lexxos::vfs::maxPathLength>* path = nullptr;
	std::array<char, storageBlockSize>* ioBuffer = nullptr;
	if (!allocateWorkspace(context, path, ioBuffer)) {
		return 1;
	}

	if (!resolvePath(context, arguments.positionalOr("path", sdBenchDefaultPath), *path)) {
		return 1;
	}

	std::uint32_t delayMs = 0;
	if (!parseDelayMs(context, arguments, delayMs)) {
		return 1;
	}

	auto storageLock = lockSdBlockDevice(context, "sd-bench");
	if (!storageLock) {
		return 1;
	}

	auto* device = sdBlockDevice(context);
	if (device == nullptr || !device->isReady()) {
		context.writeLine("sd-bench: sd0 is not ready");
		return 1;
	}

	context.write("sd-bench: active baud ");
	writeUnsigned(context, lexxos::devices::sd_card::activeBaudRate());
	context.writeLine(" Hz");

	// Keep the benchmark task stack small enough for the supervisor path.  The
	// same arena-backed 512-byte block buffer is reused for raw read, FAT write
	// and FAT read verification instead of keeping block buffers on the stack.
	std::uint32_t const rawBlocks = static_cast<std::uint32_t>(std::min<std::uint64_t>(blocks, device->blockCount()));
	std::uint32_t const rawStartMs = uptimeMs(context);
	for (std::uint32_t block = 0; block < rawBlocks; ++block) {
		if (cancellationRequested(context)) {
			return 130;
		}

		if (!device->readBlocks(block, ioBuffer->data(), 1)) {
			context.write("sd-bench: raw read failed at block ");
			writeUnsigned(context, block);
			context.writeLine("");
			return 1;
		}

		if (waitAfterBlock(context, delayMs)) {
			return 130;
		}
	}
	std::uint32_t const rawElapsedMs = elapsedMs(rawStartMs, uptimeMs(context));

	context.write("sd-bench: raw read ");
	writeUnsigned64(context, static_cast<std::uint64_t>(rawBlocks) * storageBlockSize);
	context.write(" bytes in ");
	writeUnsigned(context, rawElapsedMs);
	context.write(" ms, ");
	writeThroughput(context, static_cast<std::uint64_t>(rawBlocks) * storageBlockSize, rawElapsedMs);
	context.writeLine("");

	context.write("sd-bench: active baud after raw read ");
	writeUnsigned(context, lexxos::devices::sd_card::activeBaudRate());
	context.writeLine(" Hz");

	storageLock.release();

	context.write("sd-bench: FAT file ");
	context.write(path->data());
	context.write(", blocks=");
	writeUnsigned(context, blocks);
	context.writeLine("");

	lexxos::vfs::FileHandle output;
	auto openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::CreateTruncateWrite, output);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("sd-bench: open write failed: ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	std::uint32_t const writeStartMs = uptimeMs(context);
	for (std::uint32_t block = 0; block < blocks; ++block) {
		if (cancellationRequested(context)) {
			output.close();
			(void) lexxos::vfs::remove(path->data());
			return 130;
		}

		fillPattern(*ioBuffer, 17U, static_cast<std::uint64_t>(block) * storageBlockSize);
		auto const writeStatus = output.write(std::string_view{ioBuffer->data(), ioBuffer->size()});
		if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
			context.write("sd-bench: write failed: ");
			context.writeLine(lexxos::vfs::toString(writeStatus));
			return 1;
		}

		if (waitAfterBlock(context, delayMs)) {
			output.close();
			(void) lexxos::vfs::remove(path->data());
			return 130;
		}
	}
	output.close();

	auto const syncStatus = lexxos::vfs::sync();
	if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
		context.write("sd-bench: sync failed: ");
		context.writeLine(lexxos::vfs::toString(syncStatus));
		return 1;
	}
	std::uint32_t const writeElapsedMs = elapsedMs(writeStartMs, uptimeMs(context));

	context.write("sd-bench: FAT write ");
	writeUnsigned64(context, static_cast<std::uint64_t>(blocks) * storageBlockSize);
	context.write(" bytes in ");
	writeUnsigned(context, writeElapsedMs);
	context.write(" ms, ");
	writeThroughput(context, static_cast<std::uint64_t>(blocks) * storageBlockSize, writeElapsedMs);
	context.writeLine("");

	lexxos::vfs::FileHandle input;
	openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, input);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("sd-bench: open read failed: ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	std::uint32_t const readStartMs = uptimeMs(context);
	std::uint64_t offset = 0;
	for (std::uint32_t block = 0; block < blocks; ++block) {
		if (cancellationRequested(context)) {
			input.close();
			(void) lexxos::vfs::remove(path->data());
			return 130;
		}

		std::size_t bytesRead = 0;
		auto const readStatus = input.read(ioBuffer->data(), ioBuffer->size(), bytesRead);
		if (readStatus != lexxos::vfs::ReadStatus::Ok || bytesRead != ioBuffer->size()) {
			context.write("sd-bench: read failed at block ");
			writeUnsigned(context, block);
			context.write(": ");
			if (readStatus == lexxos::vfs::ReadStatus::Ok) {
				context.writeLine("short read");
			} else {
				context.writeLine(lexxos::vfs::toString(readStatus));
			}
			return 1;
		}

		if (!verifyPattern(*ioBuffer, bytesRead, 17U, offset)) {
			context.write("sd-bench: verify failed at block ");
			writeUnsigned(context, block);
			context.writeLine("");
			return 1;
		}

		offset += bytesRead;

		if (waitAfterBlock(context, delayMs)) {
			input.close();
			(void) lexxos::vfs::remove(path->data());
			return 130;
		}
	}
	std::uint32_t const readElapsedMs = elapsedMs(readStartMs, uptimeMs(context));

	context.write("sd-bench: FAT read/verify ");
	writeUnsigned64(context, offset);
	context.write(" bytes in ");
	writeUnsigned(context, readElapsedMs);
	context.write(" ms, ");
	writeThroughput(context, offset, readElapsedMs);
	context.writeLine("");

	input.close();
	auto const removeStatus = lexxos::vfs::remove(path->data());
	if (removeStatus != lexxos::vfs::RemoveStatus::Ok) {
		context.write("sd-bench: cleanup failed: ");
		context.writeLine(lexxos::vfs::toString(removeStatus));
		return 1;
	}

	context.writeLine("sd-bench: ok");
	return 0;
}

} // namespace lexxos::apps::storage_programs
