#include <lexxos/apps/storage/fat_cache_program.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>

#if FEATURE_ENABLED(FATFS)
	#include <lexxos/fs/fat/fatfs/fatfs_diskio.h>
#endif

namespace lexxos::apps::storage_programs {

using lexxos::program::ProgramDescriptor;

#if FEATURE_ENABLED(FATFS)
ProgramDescriptor FatCacheProgram::descriptor() {
	return {
	    .name = "fat-cache",
	    .help = "show FAT disk cache statistics",
	    .description = "Shows the FatFs diskio read-ahead cache window, hit/miss counters and disk_write chunk statistics. Use write-max to cap FAT disk_write chunks while CMD25 remains experimental.",
	    .usage = "fat-cache [clear|drop|write-max [0|1|2|4|8|16|32|64]]",
	    .handler = run,
	    .allowExtraArguments = true,
	};
}

int FatCacheProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argc > 1 && invocation.argv != nullptr) {
		std::string_view const command{invocation.argv[1]};
		if (command == "clear") {
			lexxos::fs::fat::fatfs_backend::resetDiskCacheStats();
			lexxos::fs::fat::fatfs_backend::resetDiskWriteStats();
			context.writeLine("fat-cache: counters cleared");
			return 0;
		}

		if (command == "write-max") {
			if (invocation.argc > 2 && invocation.argv != nullptr) {
				std::uint32_t sectors = 0;
				if (!parseUnsigned32(std::string_view{invocation.argv[2]}, sectors) || sectors > 64U) {
					context.writeLine("fat-cache write-max: expected 0..64 sectors, 0 means unlimited");
					return 1;
				}
				if (!lexxos::fs::fat::fatfs_backend::setDiskWriteMaxSectors(static_cast<std::size_t>(sectors))) {
					context.writeLine("fat-cache write-max: invalid sector limit");
					return 1;
				}
			}
			context.write("fat-cache: write max sectors = ");
			std::size_t const limit = lexxos::fs::fat::fatfs_backend::diskWriteMaxSectors();
			if (limit == 0U) {
				context.writeLine("unlimited");
			} else {
				writeUnsigned(context, static_cast<std::uint32_t>(limit));
				context.writeLine("");
			}
			return 0;
		}

		if (command == "drop") {
			lexxos::fs::fat::fatfs_backend::invalidateDiskCache();
			context.writeLine("fat-cache: cached window dropped");
			return 0;
		}
		context.writeLine("usage: fat-cache [clear|drop|write-max [0..64]]");
		return 1;
	}

	auto const stats = lexxos::fs::fat::fatfs_backend::diskCacheStats();
	context.write("enabled: ");
	context.writeLine(stats.enabled ? "yes" : "no");
	context.write("valid: ");
	context.writeLine(stats.valid ? "yes" : "no");
	context.write("drive: ");
	if (stats.drive == 0xFFU) {
		context.writeLine("none");
	} else {
		writeUnsigned(context, stats.drive);
		context.writeLine("");
	}
	context.write("window: first=");
	writeUnsigned64(context, stats.firstSector);
	context.write(" sectors=");
	writeUnsigned(context, stats.sectorCount);
	context.write(" capacity=");
	writeUnsigned(context, stats.capacitySectors);
	context.writeLine("");
	context.write("fills: ");
	writeUnsigned(context, stats.fills);
	context.writeLine("");
	context.write("hits: ");
	writeUnsigned(context, stats.hits);
	context.writeLine("");
	context.write("misses: ");
	writeUnsigned(context, stats.misses);
	context.writeLine("");
	context.write("device sectors: ");
	writeUnsigned(context, stats.sectorsReadFromDevice);
	context.writeLine("");
	context.write("cached sectors: ");
	writeUnsigned(context, stats.sectorsServedFromCache);
	context.writeLine("");

	auto const writes = lexxos::fs::fat::fatfs_backend::diskWriteStats();
	context.write("write max sectors: ");
	if (writes.maxSectorsPerChunk == 0U) {
		context.writeLine("unlimited");
	} else {
		writeUnsigned(context, writes.maxSectorsPerChunk);
		context.writeLine("");
	}
	context.write("write calls: ");
	writeUnsigned(context, writes.calls);
	context.write(" chunks=");
	writeUnsigned(context, writes.chunks);
	context.write(" failures=");
	writeUnsigned(context, writes.failures);
	context.writeLine("");
	context.write("write sectors: requested=");
	writeUnsigned(context, writes.requestedSectors);
	context.write(" written=");
	writeUnsigned(context, writes.writtenSectors);
	context.writeLine("");
	context.write("write max request/chunk: ");
	writeUnsigned(context, writes.maxRequestSectors);
	context.write("/");
	writeUnsigned(context, writes.maxChunkSectors);
	context.writeLine("");
	context.write("write chunks histogram: 1=");
	writeUnsigned(context, writes.chunks1);
	context.write(" 2=");
	writeUnsigned(context, writes.chunks2);
	context.write(" 3-4=");
	writeUnsigned(context, writes.chunks3To4);
	context.write(" 5-8=");
	writeUnsigned(context, writes.chunks5To8);
	context.write(" 9-16=");
	writeUnsigned(context, writes.chunks9To16);
	context.write(" 17-32=");
	writeUnsigned(context, writes.chunks17To32);
	context.write(" 33+=");
	writeUnsigned(context, writes.chunks33Plus);
	context.writeLine("");

	return 0;
}
#endif

} // namespace lexxos::apps::storage_programs
