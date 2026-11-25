#include <lexxos/apps/storage/sd_bench_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/block_read_benchmark.h>
#include <lexxos/apps/storage/detail/file_benchmark.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>
#if FEATURE_ENABLED(FATFS)
	#include <lexxos/fs/fat/fat_volume.h>
#endif
#include <algorithm>
#include <array>
#include <new>

namespace lexxos::apps::storage_programs {

namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	constexpr std::size_t bufferBytes = 4096;
	constexpr lexxos::program::ProgramArgumentSpec arguments[]{
	    {.name = "blocks", .help = "raw blocks to compare, default 128, max 1024", .kind = ProgramArgumentKind::Positional, .valueType = ProgramArgumentValueType::Unsigned},
	    {.name = "path", .help = "existing file to read, or NEW test directory with --write", .kind = ProgramArgumentKind::Positional, .valueType = ProgramArgumentValueType::Path},
	    {.name = "device", .valueName = "name", .help = "registered block device, e.g. sd0 or /dev/sd1"},
	    {.name = "start", .valueName = "lba", .help = "first raw block", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "0"},
	    {.name = "batch", .valueName = "blocks", .help = "blocks per request, 1..8", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "8"},
	    {.name = "write", .help = "create, verify and remove test data in a NEW directory", .kind = ProgramArgumentKind::Flag},
	    {.name = "bytes", .valueName = "bytes", .help = "exact test file size, default 32658, max 16777216", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "32658"},
	    {.name = "delay-ms", .shortName = 'd', .valueName = "ms", .help = "delay after each I/O request, 0..1000", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "0"},
	};

	struct Workspace {
		alignas(4) std::array<std::uint8_t, bufferBytes> reference{};
		alignas(4) std::array<std::uint8_t, bufferBytes> transfer{};
		std::array<char, lexxos::vfs::maxPathLength> path{};
	};

	struct MountLookup {
		std::string_view path{};
		std::string_view source{};
		std::string_view target{};
		bool readOnly{true};
	};

	bool findMount(lexxos::vfs::MountInfo const& mount, void* data) {
		auto& lookup = *static_cast<MountLookup*>(data);
		bool const matches = lookup.path == mount.target || (lookup.path.starts_with(mount.target) && (mount.target == "/" || (lookup.path.size() > mount.target.size() && lookup.path[mount.target.size()] == '/')));
		if (matches && mount.target.size() > lookup.target.size()) {
			lookup.source = mount.source;
			lookup.target = mount.target;
			lookup.readOnly = mount.readOnly;
		}

		return true;
	}

	std::string_view deviceName(std::string_view source) {
		return source.starts_with("/dev/") ? source.substr(5) : source;
	}

	std::uint32_t clusterSize(MountLookup const& mount) {
#if FEATURE_ENABLED(FATFS)
		struct Lookup {
			MountLookup const& mount;
			std::uint32_t bytes{};
		} lookup{mount};

		lexxos::fs::fat::visitMountedVolumes([](lexxos::fs::fat::FatVolumeInfo const& volume, void* data) {
			auto& state = *static_cast<Lookup*>(data);
			if (volume.target == state.mount.target && deviceName(volume.source) == deviceName(state.mount.source)) {
				state.bytes = volume.bytesPerCluster;
			}
			return true;
		},
		    &lookup);
		return lookup.bytes;
#else
		(void) mount;
		return 0;
#endif
	}

	bool unsignedOption(lexxos::os::AppContext& ctx, std::string_view text, std::uint32_t min,
	    std::uint32_t max, std::uint32_t& value, std::string_view name) {
		if (parseUnsigned32(text, value) && value >= min && value <= max) {
			return true;
		}

		ctx.write("sd-bench: invalid ");
		ctx.writeLine(name);
		return false;
	}

	struct ReadContext {
		lexxos::os::AppContext& ctx;
		std::uint32_t delay;
	};

	bool isCancelled(void* data) {
		return static_cast<ReadContext*>(data)->ctx.cancellationRequested();
	}

	void afterRead(void* data) {
		auto& state = *static_cast<ReadContext*>(data);
		auto left = state.delay;
		while (left && !state.ctx.cancellationRequested()) {
			auto const slice = std::min(left, static_cast<std::uint32_t>(25U));
			sleepMs(state.ctx, slice);
			left -= slice;
		}
	}

} // namespace

lexxos::program::ProgramDescriptor SdBenchProgram::descriptor() {
	return {
	    .name = "sd-bench",
	    .help = "verify block batches and file sector/cluster boundaries",
	    .description = "Portable BlockDevice/VFS benchmark. Raw access is read-only. File writes require --write and a new test directory.",
	    .usage = "sd-bench [--device name] [--start lba] [--batch 1..8] [--write] [--bytes n] [--delay-ms ms] [blocks] [path]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	    .runtime = {.launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = sizeof(Workspace),
	                .cancellable = true,
	                .allowBackground = false},
	};
}

int SdBenchProgram::run(lexxos::os::AppContext& ctx, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& args) {
	// Проверяем весь запрос до выделения ресурсов и обращения к носителю.
	std::uint32_t blocks{}, batch{}, bytes{}, delay{};
	std::uint64_t first{};
	if (!unsignedOption(ctx, args.positionalOr("blocks", "128"), 1, 1024, blocks, "blocks") || !unsignedOption(ctx, args.valueOr("batch", "8"), 1, 8, batch, "batch") || !unsignedOption(ctx, args.valueOr("bytes", "32658"), 1, 16777216, bytes, "bytes") || !unsignedOption(ctx, args.valueOr("delay-ms", "0"), 0, 1000, delay, "delay-ms")) {
		return 1;
	}

	if (!parseUnsigned64(args.valueOr("start", "0"), first)) {
		ctx.writeLine("sd-bench: invalid start LBA");
		return 1;
	}

	bool const write = args.has("write");
	bool const hasPath = !args.positional("path").empty();
	if ((write && !hasPath) || (args.has("bytes") && !write)) {
		ctx.writeLine("sd-bench: --write needs a new test directory; --bytes needs --write");
		return 1;
	}

	auto* arena = ctx.memoryArena();
	auto* memory = arena ? arena->allocate(sizeof(Workspace), alignof(Workspace)) : nullptr;
	if (!memory) {
		ctx.writeLine("sd-bench: program workspace unavailable");
		return 1;
	}

	auto& workspace = *new (memory) Workspace{};
	if (hasPath && !resolvePath(ctx, args.positional("path"), workspace.path)) {
		return 1;
	}

	MountLookup mount{.path = hasPath ? std::string_view{workspace.path.data()} : std::string_view{"/mnt/sd"}};
	ctx.vfs().visitMounts(findMount, &mount);
	auto name = deviceName(args.valueOr("device", mount.source));
	if (name.empty()) {
		ctx.writeLine("sd-bench: select --device; no mounted default device at /mnt/sd");
		return 1;
	}

	auto* device = ctx.devices().blockDevice(name);
	if (!device || !device->isReady() || ctx.vfs().blockDevice(name) != device) {
		ctx.writeLine("sd-bench: device unavailable; use block-devs and mount first");
		return 1;
	}

	if (hasPath && (mount.source.empty() || ctx.vfs().blockDevice(mount.source) != device)) {
		ctx.writeLine("sd-bench: file path must belong to the selected device");
		return 1;
	}

	if (write && (device->isReadOnly() || mount.readOnly)) {
		ctx.writeLine("sd-bench: selected filesystem is read-only");
		return 1;
	}

	// Общий тест не знает SPI/CMD18: backend сам выбирает реализацию пакетного чтения.
	auto lock = ctx.devices().lockBlockDevice(name, lexxos::devices::DeviceResourcePriority::Interactive, "sd-bench");
	if (!lock) {
		ctx.writeLine("sd-bench: device is busy");
		return 1;
	}

	ReadContext readContext{ctx, delay};
	detail::BlockReadBenchmarkOptions const options{first, blocks, batch, isCancelled, afterRead, &readContext};
	auto const started = uptimeMs(ctx);
	auto const result = detail::BlockReadBenchmark::run(*device, options, workspace.reference, workspace.transfer);
	lock.release();
	if (result.status != detail::BlockReadBenchmarkStatus::Ok) {
		if (result.status == detail::BlockReadBenchmarkStatus::Cancelled) {
			ctx.writeLine("sd-bench: cancelled");
			return 130;
		}

		ctx.write("sd-bench: raw test failed at LBA ");
		writeUnsigned64(ctx, result.failedBlock);
		ctx.write(" status=");
		ctx.writeLine(result.status == detail::BlockReadBenchmarkStatus::Mismatch ? "data-mismatch" : result.status == detail::BlockReadBenchmarkStatus::InvalidRange ? "invalid-range"
		        : result.status == detail::BlockReadBenchmarkStatus::InvalidBuffer                                                                                    ? "unsupported-block/batch-size"
		                                                                                                                                                              : "read-error");
		return 1;
	}

	auto const elapsed = std::max(static_cast<std::uint32_t>(1U), uptimeMs(ctx) - started);
	ctx.write("sd-bench: device=");
	ctx.write(name);
	ctx.write(" single/batch requests=");
	writeUnsigned(ctx, result.singleRequests);
	ctx.write("/");
	writeUnsigned(ctx, result.batchRequests);
	ctx.write(" verified bytes=");
	writeUnsigned64(ctx, result.bytesRead);
	ctx.write(" elapsed-ms=");
	writeUnsigned(ctx, elapsed);
	ctx.write(" KiB/s=");
	writeUnsigned64(ctx, result.bytesRead * 1000ULL / (elapsed * 1024ULL));
	ctx.writeLine(" (includes comparison and requested delays)");

	// Файл проверяется через тот же экземпляр VFS, на выбранном томе.
	if (hasPath) {
		auto const cluster = clusterSize(mount);
		auto buffer = std::span<char>{reinterpret_cast<char*>(workspace.transfer.data()), workspace.transfer.size()};
		auto status = detail::FileBenchmark::run(ctx, workspace.path.data(), write, bytes, cluster, delay, buffer);
		if (status) {
			return status;
		}

		// 32658 не превышает кластер 32 KiB: дополнительный проход пересекает два кластера.
		std::uint64_t const boundaryBytes = std::uint64_t(cluster) * 2U + 17U;
		if (write && cluster && bytes <= std::uint64_t(cluster) * 2U) {
			if (boundaryBytes > 16777216U) {
				ctx.writeLine("sd-bench: cluster exceeds test size limit");
				return 1;
			}

			status = detail::FileBenchmark::run(ctx, workspace.path.data(), true, static_cast<std::uint32_t>(boundaryBytes), cluster, delay, buffer);
			if (status) {
				return status;
			}
		}
	}

	ctx.writeLine("sd-bench: ok");
	return 0;
}

} // namespace lexxos::apps::storage_programs
