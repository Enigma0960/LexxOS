#include <lexxos/apps/storage/storage_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>

namespace lexxos::apps::storage_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr std::uint32_t storageWorkspaceBytes =
	    static_cast<std::uint32_t>(sizeof(StorageBlock) + alignof(StorageBlock));

	constexpr lexxos::program::ProgramArgumentSpec storageArguments[] = {
	    {
         .name = "action",
         .valueName = "action",
         .help = "optional action: probe, read0, read or diag",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "block",
         .valueName = "block",
         .help = "block number for storage read",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	};

	bool allocateSector(lexxos::os::AppContext& context, StorageBlock*& sector) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("storage: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(StorageBlock), alignof(StorageBlock));
		if (memory == nullptr) {
			context.writeLine("storage: not enough program workspace memory");
			return false;
		}

		sector = new (memory) StorageBlock{};
		return true;
	}

	void printBlockPreview(lexxos::os::AppContext& context, StorageBlock const& block) {
		context.writeLine("hex:");
		for (std::size_t row = 0; row < 4; ++row) {
			writeHex32(context, static_cast<std::uint32_t>(row * 16));
			context.write(":");
			for (std::size_t column = 0; column < 16; ++column) {
				context.write(" ");
				writeHexByte(context, block[row * 16 + column]);
			}
			context.write("  ");
			for (std::size_t column = 0; column < 16; ++column) {
				auto const value = block[row * 16 + column];
				char const character = isPrintableAscii(value) ? static_cast<char>(value) : '.';
				context.write(std::string_view{&character, 1});
			}
			context.writeLine("");
		}
	}

} // namespace

ProgramDescriptor StorageProgram::descriptor() {
	return {
	    .name = "storage",
	    .help = "show, probe, read or diagnose block storage",
	    .description = "Shows SD card information, probes storage, reads block zero/any block or runs a short diagnostic.",
	    .usage = "storage [probe|read0|read|diag] [block]",
	    .arguments = argumentList(storageArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = storageWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int StorageProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::string_view const action = arguments.positional("action");
	if (!action.empty() && action != "probe" && action != "read0" && action != "read" && action != "diag") {
		context.writeLine("usage: storage [probe|read0|read|diag] [block]");
		return 1;
	}

	auto storageLock = lockSdBlockDevice(context, "storage");
	if (!storageLock) {
		return 1;
	}

	if (action == "probe") {
		(void) probeStorage(context, storageProbeAttempts);
		return 0;
	}

	if (action == "read0") {
		StorageBlock* sector = nullptr;
		if (!allocateSector(context, sector)) {
			return 1;
		}
		(void) readStorageBlock0(context, *sector);
		return 0;
	}

	if (action == "read") {
		if (!ensureStorageReady(context)) {
			return 1;
		}

		std::uint64_t block = 0;
		if (!parseUnsigned64(arguments.positional("block"), block)) {
			context.writeLine("usage: storage read <block>");
			return 1;
		}

		auto* device = sdBlockDevice(context);
		if (device == nullptr || block >= device->blockCount()) {
			context.writeLine("storage read: block is outside device range");
			return 1;
		}

		StorageBlock* sector = nullptr;
		if (!allocateSector(context, sector)) {
			return 1;
		}
		if (!device->readBlocks(block, sector->data(), 1)) {
			context.writeLine("storage read: failed");
			return 1;
		}

		context.write("storage read: block=");
		writeUnsigned64(context, block);
		context.writeLine(" ok");
		printBlockPreview(context, *sector);
		return 0;
	}

	if (action == "diag") {
		writeStorageSummary(context);
		StorageBlock* sector = nullptr;
		if (!allocateSector(context, sector)) {
			return 1;
		}
		if (readStorageBlock0(context, *sector)) {
			printStorageBlock0Summary(context, *sector);
		}
		writeStorageSummary(context);
		return 0;
	}

	writeStorageSummary(context);
	return 0;
}

} // namespace lexxos::apps::storage_programs
