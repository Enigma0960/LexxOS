#include <lexxos/apps/core/detail/benchmark_probes.h>

#include <algorithm>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/block_read_benchmark.h>
#include <lexxos/apps/storage/detail/file_benchmark.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::system_programs::detail {

void BenchmarkMetrics::addUnsigned(std::string_view section, std::string_view metric,
    std::uint64_t value, std::string_view unit, std::string_view note) {
	char text[21]{};
	auto pos = sizeof(text) - 1;
	do {
		text[--pos] = static_cast<char>('0' + value % 10);
		value /= 10;
	} while (value);

	add(section, metric, text + pos, unit, note);
}

void sampleBenchmarkResources(lexxos::os::AppContext& context, BenchmarkMetrics& report) {
	auto const memory = lexxos::kernel::memorySnapshot();
	if (memory.available) {
		report.addUnsigned("resources", "free-heap", memory.freeHeapBytes, "bytes");
		report.addUnsigned("resources", "min-ever-free", memory.minimumEverFreeHeapBytes, "bytes", "since boot, not just this run");
	} else {
		report.add("resources", "heap", "n/a", "", "backend unavailable");
	}

	report.addUnsigned("resources", "tasks", lexxos::kernel::taskCount(), "count");
	if (auto* arena = context.memoryArena()) {
		auto const snapshot = arena->snapshot();
		report.addUnsigned("resources", "arena-internal", snapshot.reservedInternalBytes, "bytes");
		report.addUnsigned("resources", "arena-external", snapshot.reservedExternalBytes, "bytes");
	}

	lexxos::kernel::visitTasks([](lexxos::kernel::TaskSnapshot const& task, void* data) {
		auto& output = *static_cast<BenchmarkMetrics*>(data);
		output.addUnsigned("task-stack", task.name, task.stackSizeBytes, "bytes", "configured");
#if defined(LEXXOS_KERNEL_FREERTOS)
		output.addUnsigned("task-free", task.name, task.stackFreeBytes, "bytes", "backend stack watermark");
#else
        output.add("task-free", task.name, "n/a", "bytes", "backend unavailable");
#endif
		output.addUnsigned("task-priority", task.name, task.priority, "", lexxos::kernel::toString(task.executionClass));
	},
	    &report);
	report.add("resources", "cpu-utilization", "n/a", "%", "no portable runtime accounting");
	report.add("resources", "power", "n/a", "mW", "requires external measurement");
}

void sampleBenchmarkDevices(lexxos::os::AppContext& context, BenchmarkMetrics& report) {
	auto& devices = context.devices();
	report.addUnsigned("devices", "spi-buses", devices.spiBusCount(), "count");
	report.addUnsigned("devices", "uart-buses", devices.uartBusCount(), "count");
	report.addUnsigned("devices", "sensors", devices.sensorDeviceCount(), "count");
	report.add("devices", "active-other-tests", "skipped", "", "audio/sensors/network need separate tests");
	for (std::size_t i = 0; i < devices.i2cBusCount(); ++i) {
		auto const& entry = devices.i2cBusAt(i);
		report.add("i2c", entry.name, entry.bus && entry.bus->isReady() ? "ready" : "not-ready", "", "cached; no bus scan");
		if (entry.bus) {
			report.addUnsigned("i2c-pending", entry.name, entry.bus->pendingTransactions(), "count");
			report.add("i2c-busy", entry.name, entry.bus->transactionBusy() ? "yes" : "no", "");
		}
	}

	for (std::size_t i = 0; i < devices.blockDeviceCount(); ++i) {
		auto const& entry = devices.blockDeviceAt(i);
		report.add("block", entry.name, entry.device && entry.device->isReady() ? "ready" : "not-ready", "");
		if (entry.device) {
			report.addUnsigned("block-count", entry.name, entry.device->blockCount(), "blocks");
			report.addUnsigned("block-size", entry.name, entry.device->blockSizeBytes(), "bytes");
			report.add("block-ro", entry.name, entry.device->isReadOnly() ? "yes" : "no", "");
		}
	}

	for (std::size_t i = 0; i < devices.inputDeviceCount(); ++i) {
		auto const& entry = devices.inputDeviceAt(i);
		report.add("input", entry.name, entry.device && entry.device->isReady() ? "ready" : "not-ready", "", "FIFO remains owned by driver");
	}

	for (std::size_t i = 0; i < devices.displayDeviceCount(); ++i) {
		auto const& entry = devices.displayDeviceAt(i);
		report.add("display-state", entry.name, entry.device && entry.device->isReady() ? "ready" : "not-ready", "");
	}

	for (std::size_t i = 0; i < devices.picoCalcSouthbridgeServiceCount(); ++i) {
		auto const& entry = devices.picoCalcSouthbridgeServiceAt(i);
		if (!entry.service) {
			continue;
		}

		auto const& service = *entry.service;
		report.add("southbridge", entry.name, service.isReady() ? "ready" : "not-ready", "");
		report.addUnsigned("sb-updates", entry.name, service.southbridgeSnapshotUpdates(), "count", "cumulative");
		report.addUnsigned("sb-failures", entry.name, service.southbridgeSnapshotFailures(), "count", "cumulative");
		report.addUnsigned("sb-age", entry.name, uptimeMs(context) - service.southbridgeSnapshotMs(), "ms");
		report.addUnsigned("sb-stale-releases", entry.name, service.staleReleaseCount(), "count", "cumulative");
		report.addUnsigned("sb-key-count", entry.name, service.rawKeyCount(), "count", "cached");
	}
}

namespace {

	class TimedBlockDevice final : public lexxos::devices::BlockDevice {
	public:
		TimedBlockDevice(lexxos::devices::BlockDevice& device, lexxos::os::AppContext& context)
		    : device(device)
		    , context(context) {}

		bool initialize() override { return false; }

		bool isReady() const override { return device.isReady(); }

		bool isReadOnly() const override { return true; }

		std::uint32_t blockSizeBytes() const override { return device.blockSizeBytes(); }

		std::uint64_t blockCount() const override { return device.blockCount(); }

		bool writeBlocks(std::uint64_t, void const*, std::size_t) override { return false; }

		bool readBlocks(std::uint64_t block, void* data, std::size_t count) override {
			auto const start = uptimeMs(context);
			bool const ok = device.readBlocks(block, data, count);
			auto const elapsed = uptimeMs(context) - start;
			++requests;
			totalMs += elapsed;
			minimumMs = std::min(minimumMs, elapsed);
			maximumMs = std::max(maximumMs, elapsed);
			return ok;
		}

		std::uint32_t requests{}, minimumMs{UINT32_MAX}, maximumMs{};
		std::uint64_t totalMs{};

	private:
		lexxos::devices::BlockDevice& device;
		lexxos::os::AppContext& context;
	};

	struct MountLookup {
		std::string_view path, source, target;
		bool readOnly{true};
	};

	bool findMount(lexxos::vfs::MountInfo const& mount, void* data) {
		auto& result = *static_cast<MountLookup*>(data);
		if ((result.path == mount.target || (result.path.starts_with(mount.target) && (mount.target == "/" || result.path[mount.target.size()] == '/')))
		    && mount.target.size() > result.target.size()) {
			result.source = mount.source;
			result.target = mount.target;
			result.readOnly = mount.readOnly;
		}

		return true;
	}

	std::string_view bareName(std::string_view name) {
		return name.starts_with("/dev/") ? name.substr(5) : name;
	}

} // namespace

void runBenchmarkStorage(lexxos::os::AppContext& context, BenchmarkMetrics& report,
    BenchmarkProbeWorkspace& workspace, std::string_view name, std::uint64_t firstBlock,
    std::uint32_t blocks, std::string_view writeDirectory, std::uint32_t fileBytes) {
	// Never guess the boot card when no removable mount exists.
	MountLookup mount{writeDirectory.empty() ? "/mnt/sd" : writeDirectory, {}, {}};
	context.vfs().visitMounts(findMount, &mount);
	if (name.empty() && mount.source.starts_with("/dev/")) {
		name = bareName(mount.source);
	}

	name = bareName(name);
	if (name.empty()) {
		report.add("sd", "raw-read", "skipped", "", "select --device; no mounted default");
		return;
	}

	auto* device = context.devices().blockDevice(name);
	if (!device || !device->isReady()) {
		report.add("sd", "raw-read", "failed", "", "selected device unavailable");
		return;
	}

	auto lock = context.devices().lockBlockDevice(name, lexxos::devices::DeviceResourcePriority::Interactive, "bench");
	if (!lock) {
		report.add("sd", "raw-read", "failed", "", "device busy");
		return;
	}

	namespace storage = lexxos::apps::storage_programs::detail;
	auto const start = uptimeMs(context);
	TimedBlockDevice measured{*device, context};
	auto const result = storage::BlockReadBenchmark::run(measured, {
	                                                                   .firstBlock = firstBlock,
	                                                                   .blocks = blocks,
	                                                                   .batch = 8,
	                                                                   .cancelled = [](void* p) { return static_cast<lexxos::os::AppContext*>(p)->cancellationRequested(); },
	                                                                   .afterRead = [](void*) { lexxos::kernel::yield(); },
	                                                                   .userData = &context,
	                                                               },
	    workspace.reference, workspace.transfer);
	auto const elapsed = uptimeMs(context) - start;
	report.add("sd", "device", name, "");
	report.addUnsigned("sd", "start-lba", firstBlock, "blocks");
	report.addUnsigned("sd", "read-bytes", result.bytesRead, "bytes", "single + batch comparison");
	report.addUnsigned("sd", "single-requests", result.singleRequests, "count");
	report.addUnsigned("sd", "batch-requests", result.batchRequests, "count");
	report.addUnsigned("sd", "read-elapsed", elapsed, "ms");
	if (measured.requests) {
		report.addUnsigned("sd", "request-min", measured.minimumMs, "ms");
		report.addUnsigned("sd", "request-max", measured.maximumMs, "ms");
		report.addUnsigned("sd", "request-mean", measured.totalMs * 1000U / measured.requests, "us", "quantized by 1ms clock");
	}

	if (elapsed) {
		report.addUnsigned("sd", "read-rate", result.bytesRead * 1000U / elapsed / 1024U, "KiB/s");
	} else {
		report.add("sd", "read-rate", "n/a", "KiB/s", "below 1ms timer resolution");
	}

	report.addUnsigned("sd", "raw-status-code", static_cast<unsigned>(result.status), "");
	report.add("sd", "raw-read", result.status == storage::BlockReadBenchmarkStatus::Ok ? "ok" : result.status == storage::BlockReadBenchmarkStatus::Cancelled ? "cancelled"
	                                                                                                                                                           : "failed",
	    "");
	if (result.status != storage::BlockReadBenchmarkStatus::Ok) {
		report.addUnsigned("sd", "failed-lba", result.failedBlock, "blocks");
		return;
	}

	if (writeDirectory.empty()) {
		report.add("sd", "file-write", "skipped", "", "enable with --write-dir NEW-directory");
		return;
	}

	if (bareName(mount.source) != name || mount.readOnly) {
		report.add("sd", "file-write", "failed", "", "path must be on selected writable device");
		return;
	}

	auto const fileStart = uptimeMs(context);
	auto const status = storage::FileBenchmark::run(context, writeDirectory, true, fileBytes, 0, 0,
	    {reinterpret_cast<char*>(workspace.transfer.data()), workspace.transfer.size()});
	report.addUnsigned("sd", "file-elapsed", uptimeMs(context) - fileStart, "ms");
	report.add("sd", "file-verify", status == 0 ? "ok" : status == 130 ? "cancelled"
	                                                                   : "failed",
	    "");
}

} // namespace lexxos::apps::system_programs::detail
