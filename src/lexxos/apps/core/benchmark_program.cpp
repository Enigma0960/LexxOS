#include <lexxos/apps/core/benchmark_program.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <string_view>

#include <lexxos/app/console_mirror.h>
#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/core/detail/benchmark_probes.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/build_info.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	#include <lexxos/app/picocalc_lcd_console.h>
#endif

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>

	#include <lexxos/devices/bus/bus_transaction_queue.h>
	#include <lexxos/kernel/synchronization.h>

	#if defined(LEXXOS_PLATFORM_RK3506) && defined(LEXXOS_RK3506_FREERTOS)
		#include <lexxos/platform/rockchip/rk3506/irq_tick_probe.h>
	#endif
#endif

#if defined(LEXXOS_PLATFORM_RK3506)
extern "C" std::uint64_t rk3506_timer_counter();
extern "C" std::uint32_t rk3506_timer_counter_frequency();
#endif

namespace lexxos::apps::system_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr std::uint32_t defaultIterations = 200000;
	constexpr std::uint32_t minIterations = 1000;
	constexpr std::uint32_t maxIterations = 50000000;
	constexpr std::uint32_t defaultWorkers = 2;
	constexpr std::uint32_t maxWorkers = 4;
	constexpr std::uint32_t ioBenchmarkBytes = 4096;
	constexpr std::uint32_t ioChunkBytes = 512;
	constexpr std::uint32_t consoleBenchmarkLines = 16;
	constexpr std::uint32_t consoleBenchmarkFlushPasses = 8;
	constexpr std::uint32_t displayBenchmarkMaxWidth = 320;
	constexpr std::uint32_t rgb888BytesPerPixel = 3;
	constexpr std::uint32_t displayBenchmarkRows = 16;
	constexpr std::uint16_t displayBenchmarkColorA = 0x001FU;
	constexpr std::uint16_t displayBenchmarkColorB = 0x07E0U;
	constexpr std::uint16_t displayBenchmarkColorC = 0xFFFFU;
	constexpr std::uint32_t consoleClearBenchmarkFillLines = 8;
	constexpr std::uint32_t consoleClearBenchmarkFlushPasses = 4;
	constexpr std::uint32_t microOperationCount = 1000;
	constexpr std::uint32_t minMicroOperationBatches = 4;
	constexpr std::uint32_t maxMicroOperationBatches = 100;
	constexpr std::uint32_t lockBenchmarkIterations = 10000;
	constexpr std::uint32_t pingPongRounds = 1000;
	constexpr std::uint32_t rtosTickProbeSleepMs = 20;
	constexpr std::uint32_t rtosTickProbePasses = 8;
	constexpr std::uint32_t piBenchmarkDigits = 64;
	constexpr std::size_t piSpigotWorkSize = piBenchmarkDigits * 10U / 3U + 1U;

	constexpr std::string_view yesText = "yes";
	constexpr std::string_view noText = "no";
	constexpr std::string_view displayBenchmarkOwner = "bench-display";
	constexpr char defaultReportPath[] = "/mnt/sd/system-benchmark.txt";
	constexpr char defaultReportDirectory[] = "/mnt/sd";
	constexpr char defaultIoPath[] = "/tmp/bench-io.tmp";

	constexpr ProgramArgumentSpec benchmarkArguments[] = {
	    {.name = "profile", .valueName = "quick|full|soak|sync|vm|vm-detail", .help = "system, RV32I VM or detailed opcode benchmark", .defaultValue = "quick"},
	    {.name = "vm-case", .valueName = "substring", .help = "vm-detail only: select opcode cases by name substring"},
	    {.name = "vm-repeat", .valueName = "count", .help = "vm-detail only: 1..9 measurements per opcode, default 1", .valueType = ProgramArgumentValueType::Unsigned},
	    {.name = "duration", .valueName = "seconds", .help = "soak target duration, 1..86400 (default 300)", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "300"},
	    {.name = "device", .valueName = "name", .help = "block device to verify; default mounted /mnt/sd device"},
	    {.name = "sd-blocks", .valueName = "count", .help = "blocks per raw comparison, 1..1024", .valueType = ProgramArgumentValueType::Unsigned},
	    {.name = "sd-start", .valueName = "lba", .help = "first raw block (same range repeated during soak)", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "0"},
	    {.name = "write-dir", .valueName = "path", .help = "opt-in SD write/verify in a NEW directory; never raw writes", .valueType = ProgramArgumentValueType::Path},
	    {.name = "file-bytes", .valueName = "count", .help = "file write size, 1..16777216 (default 1048576)", .valueType = ProgramArgumentValueType::Unsigned, .defaultValue = "1048576"},
	    {.name = "no-display", .help = "skip graphics/console tests", .kind = ProgramArgumentKind::Flag},
	    {.name = "no-mirror", .help = "suspend LCD text mirror; requires --no-display", .kind = ProgramArgumentKind::Flag},
	    {
	     .name = "iterations",
	     .shortName = 'n',
	     .valueName = "count",
	     .help = "synthetic benchmark iterations, default: 200000",
	     .kind = ProgramArgumentKind::Option,
	     .valueType = ProgramArgumentValueType::Unsigned,
	     .required = false,
	     .defaultValue = "200000",
	     },
	    {
	     .name = "tasks",
	     .shortName = 't',
	     .valueName = "count",
	     .help = "worker task count for multitask benchmark, default: 2, max: 4",
	     .kind = ProgramArgumentKind::Option,
	     .valueType = ProgramArgumentValueType::Unsigned,
	     .required = false,
	     .defaultValue = "2",
	     },
	    {
	     .name = "out",
	     .shortName = 'o',
	     .valueName = "path",
	     .help = "report file path, default: timestamped file in /mnt/sd",
	     .kind = ProgramArgumentKind::Option,
	     .valueType = ProgramArgumentValueType::Path,
	     .required = false,
	     .defaultValue = defaultReportPath,
	     },
	    {
	     .name = "io-path",
	     .valueName = "path",
	     .help = "temporary file used by the VFS I/O benchmark, default: /tmp/bench-io.tmp",
	     .kind = ProgramArgumentKind::Option,
	     .valueType = ProgramArgumentValueType::Path,
	     .required = false,
	     .defaultValue = defaultIoPath,
	     },
	    {
	     .name = "no-save",
	     .help = "print the report without saving it to VFS",
	     .kind = ProgramArgumentKind::Flag,
	     .valueType = ProgramArgumentValueType::String,
	     .required = false,
	     },
	};

	std::size_t textLength(char const* text, std::size_t capacity) {
		std::size_t length = 0;
		while (length < capacity && text[length] != '\0') {
			++length;
		}
		return length;
	}

	template <std::size_t Size>
	std::string_view textView(std::array<char, Size> const& buffer) {
		return std::string_view{buffer.data(), textLength(buffer.data(), buffer.size())};
	}

	template <std::size_t Size>
	void copyText(std::array<char, Size>& output, std::string_view value) {
		auto const count = std::min<std::size_t>(value.size(), Size - 1U);
		std::memcpy(output.data(), value.data(), count);
		output[count] = '\0';
		for (std::size_t index = count + 1U; index < Size; ++index) {
			output[index] = '\0';
		}
	}

	std::string_view platformName() {
#if defined(LEXXOS_PLATFORM_PICO)
		return "pico";
#elif defined(LEXXOS_PLATFORM_RK3506)
		return "rk3506";
#elif defined(LEXXOS_PLATFORM_SIM)
		return "sim";
#elif defined(LEXXOS_PLATFORM_HOST)
		return "host";
#elif defined(LEXXOS_PLATFORM_STM32F4)
		return "stm32f4";
#elif defined(LEXXOS_PLATFORM_ESPIDF)
		return "espidf";
#else
		return "unknown";
#endif
	}

	std::string_view kernelName() {
#if defined(LEXXOS_KERNEL_FREERTOS)
		return "freertos";
#else
		return "host/bootstrap";
#endif
	}

	std::size_t formatUnsigned(char* output, std::size_t capacity, std::uint64_t value) {
		if (capacity == 0) {
			return 0;
		}

		char scratch[32]{};
		std::size_t scratchSize = 0;
		do {
			scratch[scratchSize++] = static_cast<char>('0' + (value % 10ULL));
			value /= 10ULL;
		} while (value != 0 && scratchSize < sizeof(scratch));

		auto const count = std::min<std::size_t>(scratchSize, capacity - 1U);
		for (std::size_t index = 0; index < count; ++index) {
			output[index] = scratch[scratchSize - index - 1U];
		}

		output[count] = '\0';
		return count;
	}

	struct FixedText {
		std::array<char, 64> data{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		void append(std::string_view value) {
			for (char const character : value) {
				if (size + 1U >= data.size()) {
					break;
				}

				data[size++] = character;
			}

			data[size] = '\0';
		}

		void appendUnsigned(std::uint64_t value) {
			char buffer[32]{};
			formatUnsigned(buffer, sizeof(buffer), value);
			append(buffer);
		}

		[[nodiscard]] std::string_view view() const {
			return std::string_view{data.data(), size};
		}
	};

	template <std::size_t Size>
	void appendToArray(std::array<char, Size>& output, std::size_t& size, std::string_view value) {
		for (char const character : value) {
			if (size + 1U >= output.size()) {
				break;
			}

			output[size++] = character;
		}

		output[size] = '\0';
	}

	void appendPaddedUnsigned(FixedText& output, std::uint32_t value, std::uint8_t width) {
		char buffer[10]{};
		auto position = width;
		buffer[position] = '\0';
		while (position > 0U) {
			--position;
			buffer[position] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		}

		output.append(std::string_view{buffer, width});
	}

	void formatRtcTimestamp(lexxos::devices::rtc::DateTime const& value, FixedText& output) {
		appendPaddedUnsigned(output, value.year, 4);
		appendPaddedUnsigned(output, value.month, 2);
		appendPaddedUnsigned(output, value.day, 2);
		output.append("-");
		appendPaddedUnsigned(output, value.hour, 2);
		appendPaddedUnsigned(output, value.minute, 2);
		appendPaddedUnsigned(output, value.second, 2);
	}

	bool readCurrentDateTime(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    lexxos::devices::rtc::DateTime& output) {
		lexxos::apps::rtc_programs::detail::RtcBackend backend{};
		auto silentContext = context.createChild({
		    .standardOutput = &lexxos::io::nullOutputStream(),
		    .standardError = &lexxos::io::nullOutputStream(),
		});
		return lexxos::apps::rtc_programs::detail::openRtcBackend(silentContext, "bench", arguments, backend)
		    && lexxos::apps::rtc_programs::detail::readDateTime(backend, output);
	}

	FixedText makeReportTimestamp(lexxos::os::AppContext& context, lexxos::program::ParsedArguments const& arguments) {
		FixedText timestamp;
		lexxos::devices::rtc::DateTime now{};
		if (readCurrentDateTime(context, arguments, now)) {
			formatRtcTimestamp(now, timestamp);
			return timestamp;
		}

		timestamp.append("uptime-");
		timestamp.appendUnsigned(uptimeMs(context));
		timestamp.append("ms");
		return timestamp;
	}

	void buildTimestampedReportPath(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view directory,
	    std::array<char, lexxos::vfs::maxPathLength>& output) {
		std::size_t size = 0;
		output[0] = '\0';
		auto const timestamp = makeReportTimestamp(context, arguments);
		appendToArray(output, size, directory);
		appendToArray(output, size, "/system-benchmark-");
		appendToArray(output, size, timestamp.view());
		appendToArray(output, size, ".txt");
	}

	struct BenchmarkRow {
		std::array<char, 20> section{};
		std::array<char, 32> metric{};
		std::array<char, 96> value{};
		std::array<char, 16> unit{};
		std::array<char, 96> note{};
	};

	struct BenchmarkReport final : detail::BenchmarkMetrics {
		static constexpr std::size_t maxRows = 160;

		BenchmarkReport(BenchmarkRow* buffer, std::size_t capacity)
		    : rows(buffer)
		    , capacity(capacity) {
		}

		BenchmarkRow* rows;
		std::size_t capacity;
		std::size_t rowCount{0};
		unsigned failures{}, skipped{}, dropped{};

		void add(std::string_view section, std::string_view metric, std::string_view value, std::string_view unit, std::string_view note = {}) override {
			if (value == "failed" || value == "lock-failed" || value == "invalid" || value == "not-ready") {
				++failures;
			}

			if (value == "skipped" || value == "disabled" || value == "n/a" || value == "none") {
				++skipped;
			}

			if (rowCount >= capacity) {
				++dropped;
				return;
			}

			auto& row = rows[rowCount++];
			copyText(row.section, section);
			copyText(row.metric, metric);
			copyText(row.value, value);
			copyText(row.unit, unit);
			copyText(row.note, note);
		}

		void addUnsigned(std::string_view section, std::string_view metric, std::uint64_t value, std::string_view unit,
		    std::string_view note = {}) {
			char buffer[32]{};
			formatUnsigned(buffer, sizeof(buffer), value);
			add(section, metric, buffer, unit, note);
		}
	};

	struct BenchmarkWorker {
		std::atomic<bool>* start{nullptr};
		std::atomic<bool> completed{false};
		std::uint32_t iterations{0};
		std::uint32_t checksum{0};
	};

	enum class MultitaskPlacement : std::uint8_t {
		Default,
		Core0,
		Split,
	};

#if defined(LEXXOS_KERNEL_FREERTOS)
	struct RtosTickProbeState {
		std::uint32_t (*uptimeMs)(){nullptr};
		std::uint32_t sleepMs{0};
		std::uint32_t passes{0};
		std::uint32_t elapsed{0};
		std::atomic<bool> completed{false};
	};
#endif

	struct BenchmarkWorkspace {
		std::array<BenchmarkRow, BenchmarkReport::maxRows> reportRows{};
		BenchmarkReport report{reportRows.data(), reportRows.size()};
		detail::BenchmarkProbeWorkspace probes{};
		std::array<char, lexxos::vfs::maxPathLength> reportPath{};
		std::array<char, lexxos::vfs::maxPathLength> ioPath{};
		std::array<char, ioChunkBytes> ioBuffer{};
		std::array<std::uint32_t, piSpigotWorkSize> piWork{};
		std::array<std::uint16_t, displayBenchmarkMaxWidth> displayPixels{};
		std::array<std::uint8_t, displayBenchmarkMaxWidth * rgb888BytesPerPixel> displayRgb888Row{};
		std::atomic<bool> workersStart{false};
		std::array<BenchmarkWorker, maxWorkers> workers{};
	};

	struct SyncBenchmarkWorkspace {
		// Each sync report is flushed between phases; 64 rows cover the
		// largest resource snapshot without allocating CPU/SD/LCD buffers.
		std::array<BenchmarkRow, 64> reportRows{};
		BenchmarkReport report{reportRows.data(), reportRows.size()};
		std::array<char, lexxos::vfs::maxPathLength> reportPath{};
	};

	struct VmBenchmarkWorkspace {
		std::array<BenchmarkRow, 64> reportRows{};
		BenchmarkReport report{reportRows.data(), reportRows.size()};
		std::array<char, lexxos::vfs::maxPathLength> reportPath{};
		alignas(4) std::array<std::uint8_t, 128> vmMemory{};
	};

	std::uint64_t vmClockTicks() {
#if defined(LEXXOS_PLATFORM_RK3506)
		return rk3506_timer_counter(); // CNTVCT, not CPU cycles.
#else
		return 0;
#endif
	}

	std::uint32_t vmClockFrequency() {
#if defined(LEXXOS_PLATFORM_RK3506)
		return rk3506_timer_counter_frequency();
#else
		return 0;
#endif
	}

	std::uint64_t vmElapsedNs(std::uint64_t start, std::uint64_t end, std::uint32_t frequency) {
		if (frequency == 0U) {
			return 0;
		}

		auto const elapsed = end - start;
		return (elapsed / frequency) * 1000000000ULL
		    + ((elapsed % frequency) * 1000000000ULL) / frequency;
	}

	class ReportOutputStream final : public lexxos::io::OutputStream {
	public:
		ReportOutputStream(lexxos::io::OutputStream& console, lexxos::vfs::FileHandle* file)
		    : _console(console)
		    , _file(file) {
		}

		bool write(std::string_view text) override {
			auto const consoleOk = _console.write(text);
			_consoleFailed = _consoleFailed || !consoleOk;
			if (_file != nullptr && _file->isOpen() && !_failed) {
				auto const status = _file->write(text);
				if (status != lexxos::vfs::WriteStatus::Ok) {
					_failed = true;
				}
			}

			return consoleOk && !_failed;
		}

		[[nodiscard]] bool failed() const {
			return _failed || _consoleFailed;
		}

	private:
		lexxos::io::OutputStream& _console;
		lexxos::vfs::FileHandle* _file{nullptr};
		bool _failed{false};
		bool _consoleFailed{};
	};

	std::uint32_t elapsedMs(std::uint32_t start, std::uint32_t end) {
		if (end >= start) {
			return end - start;
		}
		return (std::numeric_limits<std::uint32_t>::max() - start) + end + 1U;
	}

	std::uint32_t nonZeroElapsed(std::uint32_t elapsed) {
		return elapsed == 0 ? 1U : elapsed;
	}

	std::uint64_t perSecond(std::uint64_t count, std::uint32_t elapsed) {
		return (count * 1000ULL) / nonZeroElapsed(elapsed);
	}

	std::uint64_t kibPerSecond(std::uint64_t bytes, std::uint32_t elapsed) {
		return ((bytes * 1000ULL) / nonZeroElapsed(elapsed)) / 1024ULL;
	}

	std::uint32_t schedulerCoreCount() {
#if defined(LEXXOS_KERNEL_FREERTOS) && defined(configNUMBER_OF_CORES)
		return static_cast<std::uint32_t>(configNUMBER_OF_CORES);
#elif defined(LEXXOS_PLATFORM_ESPIDF) && defined(CONFIG_FREERTOS_UNICORE) && !CONFIG_FREERTOS_UNICORE
		return 2;
#else
		return 1;
#endif
	}

	bool schedulerHasCoreAffinity() {
#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(CONFIG_FREERTOS_UNICORE)
		return !CONFIG_FREERTOS_UNICORE;
#elif defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
		return true;
#else
		return false;
#endif
	}

	[[maybe_unused]] void appendCoreNote(FixedText& note, int coreAffinity) {
		note.append("core=");
		if (coreAffinity < 0) {
			note.append("default");
		} else {
			note.appendUnsigned(static_cast<std::uint32_t>(coreAffinity));
		}
	}

	int multitaskWorkerCore(MultitaskPlacement placement, std::uint32_t workerIndex) {
		switch (placement) {
			case MultitaskPlacement::Default:
				return lexxos::kernel::anyTaskCore;
			case MultitaskPlacement::Core0:
				return 0;
			case MultitaskPlacement::Split:
				return static_cast<int>(workerIndex % schedulerCoreCount());
		}

		return lexxos::kernel::anyTaskCore;
	}

	std::string_view multitaskPlacementName(MultitaskPlacement placement) {
		switch (placement) {
			case MultitaskPlacement::Default:
				return "default";
			case MultitaskPlacement::Core0:
				return "core0";
			case MultitaskPlacement::Split:
				return "split";
		}

		return "default";
	}

	std::uint32_t runIntegerLoop(std::uint32_t iterations) {
		std::uint32_t value = 0x12345678U;
		std::uint32_t volatile sink = 0;
		for (std::uint32_t index = 0; index < iterations; ++index) {
			value = value * 1664525U + 1013904223U;
			value ^= value >> 16U;
			sink ^= value + index;
		}

		return sink;
	}

	void benchmarkWorkerEntry(void* userData) {
		auto& worker = *static_cast<BenchmarkWorker*>(userData);
		while (!worker.start->load(std::memory_order_acquire)) {
			lexxos::kernel::yield();
		}

		worker.checksum = runIntegerLoop(worker.iterations);
		worker.completed.store(true, std::memory_order_release);
	}

	void addClockRate(BenchmarkReport& report, std::string_view metric, lexxos::platform::ClockRate rate) {
		if (!rate.available) {
			report.add("clock", metric, "n/a", "Hz");
			return;
		}
		report.addUnsigned("clock", metric, rate.hz, "Hz");
	}

	void addConfiguredBusRates(BenchmarkReport& report, lexxos::platform::LegacyBoardDescriptor const& board) {
		if (board.debugUart.isValid()) {
			report.addUnsigned("bus", "debug-uart", board.debugUart.baudRate, "baud");
		}

		if (board.debugOledI2c.isValid()) {
			report.addUnsigned("bus", "debug-oled-i2c", board.debugOledI2c.baudRate, "Hz");
		}

		if (board.interfaces.systemI2c.isValid()) {
			report.addUnsigned("bus", "system-i2c", board.interfaces.systemI2c.baudRate, "Hz");
		}

		if (board.interfaces.storageSpi.isTxValid()) {
			report.addUnsigned("bus", "storage-spi", board.interfaces.storageSpi.baudRate, "Hz");
		} else if (board.sdSpi.isValid()) {
			report.addUnsigned("bus", "sd-spi", board.sdSpi.baudRate, "Hz");
		}

		if (board.interfaces.displaySpi.isTxValid()) {
			report.addUnsigned("bus", "display-spi", board.interfaces.displaySpi.baudRate, "Hz");
		}

		report.add("bus", "sd-interface", lexxos::platform::toString(board.sdInterface), "", "board descriptor");
	}

	void collectConfiguration(lexxos::os::AppContext& context, BenchmarkReport& report) {
		auto const& board = activeBoard(context);
		report.add("config", "version", lexxos::version, "");
		report.add("config", "build-date", lexxos::buildDate, "");
		report.add("config", "build-time", lexxos::buildTime, "");
		report.add("config", "platform", platformName(), "");
		report.add("config", "kernel", kernelName(), "");
		report.add("config", "board", board.boardId != nullptr ? board.boardId : "unknown", "");
		report.add("config", "board-name", board.boardName != nullptr ? board.boardName : "unknown", "");
		report.add("config", "processor", board.processorName != nullptr ? board.processorName : "unknown", "");
		report.add("config", "processor-family", lexxos::platform::toString(board.processorFamily), "");
#ifdef LEXXOS_BOARD_NAME
		report.add("config", "cmake-board", LEXXOS_BOARD_NAME, "");
#endif

		auto const performance = lexxos::platform::performanceSnapshot();
		addClockRate(report, "sys", performance.systemClock);
		addClockRate(report, "peri", performance.peripheralClock);
		addClockRate(report, "ref", performance.referenceClock);
		addClockRate(report, "usb", performance.usbClock);
		addClockRate(report, "adc", performance.adcClock);
		addClockRate(report, "rtc", performance.rtcClock);

		addConfiguredBusRates(report, board);

		auto const memory = lexxos::kernel::memorySnapshot();
		if (memory.available) {
			report.addUnsigned("memory", "free-heap", memory.freeHeapBytes, "bytes");
			report.addUnsigned("memory", "min-ever-free", memory.minimumEverFreeHeapBytes, "bytes");
		} else {
			report.add("memory", "heap", "n/a", "", "backend does not expose heap stats");
		}

		report.addUnsigned("scheduler", "active-tasks", lexxos::kernel::taskCount(), "tasks");
		report.addUnsigned("scheduler", "cores", schedulerCoreCount(), "cores");
		report.add("scheduler", "core-affinity", schedulerHasCoreAffinity() ? "on" : "off", "");
#if defined(configTICK_CORE)
		report.addUnsigned("scheduler", "tick-core", static_cast<std::uint32_t>(configTICK_CORE), "core");
#endif
#if defined(configTASK_DEFAULT_CORE_AFFINITY)
		report.addUnsigned("scheduler", "default-mask", static_cast<std::uint32_t>(configTASK_DEFAULT_CORE_AFFINITY), "mask", "unqualified tasks");
#endif
	}

	void runCpuBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, std::uint32_t iterations) {
		auto const start = uptimeMs(context);
		auto const checksum = runIntegerLoop(iterations);
		auto const elapsed = elapsedMs(start, uptimeMs(context));

		report.addUnsigned("sync", "integer-loop", elapsed, "ms");
		report.addUnsigned("sync", "integer-rate", perSecond(iterations, elapsed), "iter/s");
		FixedText note;
		note.append("checksum=");
		note.appendUnsigned(checksum);
		report.addUnsigned("sync", "iterations", iterations, "count", note.view());
	}

	void runVmBenchmark(lexxos::os::AppContext& context,
	    BenchmarkReport& report,
	    VmBenchmarkWorkspace& workspace,
	    std::uint32_t iterations) {
		using lexxos::external_program::Rv32iBenchmarkCase;
		using lexxos::external_program::Rv32iHostBenchmarkCase;

		struct Case {
			char const* name;
			Rv32iBenchmarkCase kind;
			std::uint32_t perIteration;
			std::uint32_t fixedInstructions;
			bool returnsCounter;
		};

		constexpr Case cases[] = {
		    {   "alu",          Rv32iBenchmarkCase::Alu, 5, 5,  true},
		    {"branch",       Rv32iBenchmarkCase::Branch, 4, 5,  true},
		    {"memory",       Rv32iBenchmarkCase::Memory, 5, 6,  true},
		    {  "zero", Rv32iBenchmarkCase::ZeroRegister, 3, 6,  true},
		    { "ecall",        Rv32iBenchmarkCase::Ecall, 3, 6, false},
		};

		report.addUnsigned("rv32i", "loop-iterations",
		    iterations, "count");
		report.add("rv32i", "backend", "interpreter", "",
		    "production RV32I VM; no VFS or display calls");

#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
		report.add("rv32i", "aligned-access", "on", "", "native 16/32-bit load/store");
#else
		report.add("rv32i", "aligned-access", "off", "", "bytewise little-endian load/store");
#endif

#if defined(LEXXOS_VM_FAST_LOOP)
		report.add("rv32i", "fast-loop", "on", "", "batched checks; x0 bypass");
#else
		report.add("rv32i", "fast-loop", "off", "", "legacy loop");
#endif

#if defined(LEXXOS_VM_LOCAL_PC)
		report.add("rv32i", "local-pc", "on", "",
		    "PC committed on ECALL, trace and VM exit");
#else
		report.add("rv32i", "local-pc", "off", "",
		    "PC stored in VM state");
#endif

#if defined(LEXXOS_VM_EXECUTION_PROFILE)
	#if LEXXOS_VM_EXECUTION_PROFILE == 0
		report.add("rv32i", "execution-profile", "compact", "");
	#elif LEXXOS_VM_EXECUTION_PROFILE == 1
		report.add("rv32i", "execution-profile", "fast", "");
	#else
		report.add("rv32i", "execution-profile", "cached", "");
	#endif
		report.addUnsigned("rv32i", "decode-cache-entries",
		    LEXXOS_VM_DECODE_CACHE_ENTRIES, "entries");
#endif

#if defined(LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL)
		report.addUnsigned("rv32i", "compiler-optimization",
		    LEXXOS_RK3506_VM_OPTIMIZATION_LEVEL, "level");
#else
		report.add("rv32i", "compiler-optimization",
		    "project-default", "");
#endif

		struct HostCase {
			char const* name;
			Rv32iHostBenchmarkCase kind;
		};

		constexpr HostCase hostCases[] = {
		    {    "host-reg",  Rv32iHostBenchmarkCase::RegisterAlu},
		    {"host-ddr-rmw", Rv32iHostBenchmarkCase::DdrReadWrite},
		};

		for (auto const& test : hostCases) {
			if (context.cancellationRequested()) {
				break;
			}

			auto const start = uptimeMs(context);

			// Scale the CPU-only case above the 1 ms timer resolution.
			auto const hostIterations = test.kind == Rv32iHostBenchmarkCase::RegisterAlu
			    ? iterations * 64U
			    : iterations;

			auto const checksum =
			    lexxos::external_program::benchmarkRv32iHost(
			        test.kind, hostIterations);
			auto const elapsed = elapsedMs(start, uptimeMs(context));

			FixedText elapsedName;
			elapsedName.append(test.name);
			elapsedName.append("-elapsed");
			report.addUnsigned("rv32i", elapsedName.view(),
			    elapsed, "ms");

			FixedText rateName;
			rateName.append(test.name);
			rateName.append("-rate");

			FixedText note;
			note.append("checksum=");
			note.appendUnsigned(checksum);

			if (elapsed != 0U) {
				report.addUnsigned("rv32i", rateName.view(),
				    perSecond(hostIterations, elapsed),
				    "iter/s", note.view());
			} else {
				report.add("rv32i", rateName.view(),
				    "n/a", "iter/s", "increase --iterations");
			}
		}

		for (auto const& test : cases) {
			if (context.cancellationRequested()) {
				break;
			}

			auto const start = uptimeMs(context);
			auto const result = lexxos::external_program::benchmarkRv32i(
			    context, test.kind, iterations,
			    workspace.vmMemory.data(), workspace.vmMemory.size());
			auto const elapsed = elapsedMs(start, uptimeMs(context));

			auto const expectedInstructions =
			    static_cast<std::uint64_t>(iterations) * test.perIteration
			    + test.fixedInstructions;
			auto const expectedExit =
			    test.returnsCounter ? static_cast<int>(iterations) : 0;

			if (result.status != lexxos::external_program::ExternalProgramStatus::Ok
			    || result.exitCode != expectedExit
			    || result.instructionsExecuted != expectedInstructions) {
				report.add("rv32i", test.name, "failed", "",
				    result.status == lexxos::external_program::ExternalProgramStatus::Ok
				        ? "checksum or instruction count mismatch"
				        : lexxos::external_program::toString(result.status));
				continue;
			}

			auto add = [&](std::string_view suffix,
			               std::uint64_t value,
			               std::string_view unit) {
				FixedText metric;
				metric.append(test.name);
				metric.append(suffix);
				report.addUnsigned("rv32i", metric.view(), value, unit);
			};

			add("-instructions", result.instructionsExecuted, "inst");
			add("-elapsed", elapsed, "ms");

			if (elapsed != 0) {
				add("-rate",
				    perSecond(result.instructionsExecuted, elapsed),
				    "inst/s");
				add("-ns-per-inst",
				    static_cast<std::uint64_t>(elapsed) * 1000000ULL / result.instructionsExecuted,
				    "ns/inst");
			} else {
				FixedText metric;
				metric.append(test.name);
				metric.append("-rate");
				report.add("rv32i", metric.view(), "n/a", "inst/s",
				    "elapsed below 1ms; increase --iterations");
			}
		}
	}

	template <typename Flush>
	void runVmDetailedBenchmark(
	    lexxos::os::AppContext& context,
	    BenchmarkReport& report,
	    VmBenchmarkWorkspace& workspace,
	    std::uint32_t iterations,
	    std::string_view caseFilter,
	    std::uint32_t repeats,
	    Flush&& flush) {
		using Form = lexxos::external_program::Rv32iOpcodeForm;
		using Probe = lexxos::external_program::Rv32iOpcodeProbe;

		struct Case {
			char const* name;
			Probe probe;
		};

		constexpr Case cases[] = {
		    // Baseline VM dispatch: only decrement and conditional branch.
		    {     "control.loop",{Form::LoopControl, 0, 0, 0, 0, 0, 0}                                 },
		    // OP: register-register arithmetic.
		    {           "op.add",        {Form::Register, 0, 0, 1, 0, 0, 1}},
		    {           "op.sub",     {Form::Register, 0, 0x20, 2, 1, 0, 1}},
		    {           "op.sll",        {Form::Register, 1, 0, 1, 1, 0, 2}},
		    {           "op.slt",       {Form::Register, 2, 0, -1, 0, 0, 1}},
		    {          "op.sltu",        {Form::Register, 3, 0, 0, 1, 0, 1}},
		    {           "op.xor",        {Form::Register, 4, 0, 0, 1, 0, 1}},
		    {           "op.srl",        {Form::Register, 5, 0, 2, 1, 0, 1}},
		    {           "op.sra",   {Form::Register, 5, 0x20, -2, 1, 0, -1}},
		    {            "op.or",        {Form::Register, 6, 0, 0, 1, 0, 1}},
		    {           "op.and",        {Form::Register, 7, 0, 1, 1, 0, 1}},

		    // OP-IMM: arithmetic with immediate operands.
		    {         "imm.addi",       {Form::Immediate, 0, 0, 0, 0, 1, 1}},
		    {         "imm.slti",       {Form::Immediate, 2, 0, 0, 0, 1, 1}},
		    {        "imm.sltiu",       {Form::Immediate, 3, 0, 0, 0, 1, 1}},
		    {         "imm.xori",       {Form::Immediate, 4, 0, 0, 0, 1, 1}},
		    {          "imm.ori",       {Form::Immediate, 6, 0, 0, 0, 1, 1}},
		    {         "imm.andi",       {Form::Immediate, 7, 0, 1, 0, 1, 1}},
		    {         "imm.slli",       {Form::Immediate, 1, 0, 1, 0, 1, 2}},
		    {         "imm.srli",       {Form::Immediate, 5, 0, 2, 0, 1, 1}},
		    {         "imm.srai", {Form::Immediate, 5, 0, -2, 0, 0x401, -1}},

		    // LOAD: signed and unsigned memory access.
		    {          "load.lb",      {Form::Load, 0, 0, 100, 0, 0xFF, -1}},
		    {          "load.lh",    {Form::Load, 1, 0, 100, 0, 0xFFFF, -1}},
		    {          "load.lw",          {Form::Load, 2, 0, 100, 0, 1, 1}},
		    {         "load.lbu",     {Form::Load, 4, 0, 100, 0, 0xFF, 255}},
		    {         "load.lhu", {Form::Load, 5, 0, 100, 0, 0xFFFF, 65535}},

		    // STORE, followed by a validating read.
		    {         "store.sb",         {Form::Store, 0, 0, 100, 1, 0, 1}},
		    {         "store.sh",         {Form::Store, 1, 0, 100, 1, 0, 1}},
		    {         "store.sw",         {Form::Store, 2, 0, 100, 1, 0, 1}},

		    // BRANCH: both taken and not-taken paths.
		    {        "beq.taken",          {Form::Branch, 0, 0, 0, 0, 0, 1}},
		    {          "beq.not",          {Form::Branch, 0, 0, 0, 1, 0, 2}},
		    {        "bne.taken",          {Form::Branch, 1, 0, 0, 1, 0, 1}},
		    {          "bne.not",          {Form::Branch, 1, 0, 0, 0, 0, 2}},
		    {        "blt.taken",         {Form::Branch, 4, 0, -1, 0, 0, 1}},
		    {          "blt.not",          {Form::Branch, 4, 0, 0, 0, 0, 2}},
		    {        "bge.taken",          {Form::Branch, 5, 0, 0, 0, 0, 1}},
		    {          "bge.not",         {Form::Branch, 5, 0, -1, 0, 0, 2}},
		    {       "bltu.taken",          {Form::Branch, 6, 0, 0, 1, 0, 1}},
		    {         "bltu.not",          {Form::Branch, 6, 0, 1, 0, 0, 2}},
		    {       "bgeu.taken",          {Form::Branch, 7, 0, 1, 0, 0, 1}},
		    {         "bgeu.not",          {Form::Branch, 7, 0, 0, 1, 0, 2}},

		    // Control flow and remaining RV32I opcode families.
		    {         "jump.jal",            {Form::Jump, 0, 0, 0, 0, 0, 1}},
		    {        "jump.jalr",   {Form::JumpRegister, 0, 0, 24, 0, 0, 1}},
		    {        "upper.lui",       {Form::LoadUpper, 0, 0, 0, 0, 0, 1}},
		    {      "upper.auipc",      {Form::AddUpperPc, 0, 0, 0, 0, 0, 1}},
		    {       "sync.fence",           {Form::Fence, 0, 0, 0, 0, 0, 1}},

		    // Mixed workloads.
		    {   "mix.alu-memory",
		     {Form::MixedAluMemory, 0, 0, 100, 0, 0, 1}                    },
		    {"mix.branch-memory",
		     {Form::MixedBranchMemory, 0, 0, 100, 1, 0, 1}                 },
		    {    "mix.ecall-alu",
		     {Form::MixedSyscall, 0, 0, 0, 0, 0, 1}                        },
		};

		report.addUnsigned("rv32i-detail", "cases",
		    sizeof(cases) / sizeof(cases[0]), "count");
		report.add("rv32i-detail", "case-filter",
		    caseFilter.empty() ? "all" : caseFilter, "");
		report.addUnsigned("rv32i-detail", "repeats", repeats, "count");

		auto const counterFrequency = vmClockFrequency();
		report.addUnsigned("rv32i-detail", "timer-frequency", counterFrequency, "Hz",
		    counterFrequency ? "CNTVCT; not CPU core cycles" : "millisecond fallback");

		std::uint32_t processed = 0;
		std::uint32_t selected = 0;
		constexpr std::uint32_t maxRepeats = 9;
		std::array<std::uint32_t, maxRepeats> elapsedSamples{};
		std::array<std::uint64_t, maxRepeats> elapsedNsSamples{};

		for (auto const& test : cases) {
			if (context.cancellationRequested()) {
				break;
			}

			if (!caseFilter.empty()
			    && std::string_view{test.name}.find(caseFilter) == std::string_view::npos) {
				continue;
			}
			++selected;

			std::uint32_t completed = 0;
			std::uint32_t instructionCount = 0;
			std::string_view errorNote = "";
			bool valid = true;
			for (std::uint32_t repetition = 0; repetition < repeats; ++repetition) {
				if (context.cancellationRequested()) {
					valid = false;
					errorNote = "cancelled";
					break;
				}

				auto const start = uptimeMs(context);
				auto const startTicks = vmClockTicks();
				auto const result =
				    lexxos::external_program::benchmarkRv32iOpcode(
				        context, test.probe, iterations,
				        workspace.vmMemory.data(), workspace.vmMemory.size());
				auto const endTicks = vmClockTicks();

				elapsedSamples[repetition] = elapsedMs(start, uptimeMs(context));
				elapsedNsSamples[repetition] = vmElapsedNs(startTicks, endTicks, counterFrequency);

				auto const expectedInstructions =
				    static_cast<std::uint64_t>(iterations)
				        * result.instructionsPerIteration
				    + result.fixedInstructions;
				auto const& program = result.program;
				valid =
				    program.status == lexxos::external_program::ExternalProgramStatus::Ok
				    && program.exitCode == result.expectedExit
				    && program.instructionsExecuted == expectedInstructions;
				if (!valid) {
					errorNote = program.status == lexxos::external_program::ExternalProgramStatus::Ok
					    ? "exit code or instruction count mismatch"
					    : lexxos::external_program::toString(program.status);
					break;
				}

				instructionCount = program.instructionsExecuted;
				++completed;
			}

			if (!valid) {
				if (context.cancellationRequested()) {
					break;
				}

				report.add("rv32i-detail", test.name, "failed", "", errorNote);
			} else {
				std::sort(elapsedSamples.begin(), elapsedSamples.begin() + completed);
				std::sort(elapsedNsSamples.begin(), elapsedNsSamples.begin() + completed);
				auto const median = static_cast<std::uint32_t>(
				    (static_cast<std::uint64_t>(elapsedSamples[(completed - 1U) / 2U])
				        + elapsedSamples[completed / 2U])
				    / 2U);
				auto const medianNs = (elapsedNsSamples[(completed - 1U) / 2U]
				                          + elapsedNsSamples[completed / 2U])
				    / 2U;

				FixedText name;

				name.append(test.name);
				name.append(".instructions");
				report.addUnsigned("rv32i-detail", name.view(),
				    instructionCount, "inst");

				name.clear();
				name.append(test.name);
				name.append(".inst-rate");
				report.addUnsigned("rv32i-detail", name.view(),
				    medianNs ? (static_cast<std::uint64_t>(instructionCount) * 1000000000ULL) / medianNs
				             : perSecond(instructionCount, median),
				    "inst/s");

				if (medianNs != 0U && instructionCount != 0U) {
					name.clear();
					name.append(test.name);
					name.append(".ns-per-inst");
					report.addUnsigned("rv32i-detail", name.view(), medianNs / instructionCount, "ns/inst");
				}

				name.clear();
				name.append(test.name);
				name.append(".loop-rate");
				report.addUnsigned("rv32i-detail", name.view(),
				    perSecond(iterations, median), "loops/s");

				name.clear();
				name.append(test.name);
				name.append(".ms-min");
				report.addUnsigned("rv32i-detail", name.view(), elapsedSamples[0], "ms");

				name.clear();
				name.append(test.name);
				name.append(".ms-median");
				report.addUnsigned("rv32i-detail", name.view(), median, "ms");

				name.clear();
				name.append(test.name);
				name.append(".ms-max");
				report.addUnsigned("rv32i-detail", name.view(),
				    elapsedSamples[completed - 1U], "ms");

				if (medianNs != 0U) {
					name.clear();
					name.append(test.name);
					name.append(".ns-median");
					report.addUnsigned("rv32i-detail", name.view(), medianNs, "ns");
				}
			}

			// Up to eight metric rows per case; five cases fit VM report buffer.
			if (++processed % 5U == 0U) {
				flush();
			}
		}

		if (selected == 0U) {
			report.add("rv32i-detail", "case-selection", "failed", "",
			    "no cases match --vm-case");
		}

		report.addUnsigned("rv32i-detail", "cases-selected", selected, "count");

		flush();
	}

	void runMemoryBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace, std::uint32_t iterations) {
		std::uint32_t const passes = std::max<std::uint32_t>(iterations / 16U, 1U);
		std::array<char, ioChunkBytes> target{};
		for (std::size_t index = 0; index < workspace.ioBuffer.size(); ++index) {
			workspace.ioBuffer[index] = static_cast<char>((index * 17U) & 0x7FU);
		}

		auto const start = uptimeMs(context);
		for (std::uint32_t pass = 0; pass < passes; ++pass) {
			std::memcpy(target.data(), workspace.ioBuffer.data(), target.size());
			std::atomic_signal_fence(std::memory_order_seq_cst);
			target[pass % target.size()] ^= static_cast<char>(pass & 0x7FU);
		}

		auto const elapsed = elapsedMs(start, uptimeMs(context));

		auto const bytes = static_cast<std::uint64_t>(passes) * target.size();
		report.addUnsigned("sync", "memory-copy", elapsed, "ms");
		report.addUnsigned("sync", "memory-copy-rate", perSecond(bytes / 1024ULL, elapsed), "KiB/s");
	}

	void runYieldBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, std::uint32_t iterations) {
		std::uint32_t const yields = std::min<std::uint32_t>(std::max<std::uint32_t>(iterations / 50U, 100U), 5000U);
		auto const start = uptimeMs(context);
		for (std::uint32_t index = 0; index < yields; ++index) {
			lexxos::kernel::yield();
		}

		auto const elapsed = elapsedMs(start, uptimeMs(context));

		report.addUnsigned("scheduler", "yield-loop", elapsed, "ms");
		report.addUnsigned("scheduler", "yield-rate", perSecond(yields, elapsed), "yield/s");
	}

	void runLatencyBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, unsigned samples) {
		std::uint32_t minimum = UINT32_MAX, maximum = 0, over10ms = 0, measured = 0;
		std::uint64_t total = 0;
		for (unsigned i = 0; i < samples && !context.cancellationRequested(); ++i) {
			auto const start = uptimeMs(context);
			sleepMs(context, 1);
			auto const elapsed = elapsedMs(start, uptimeMs(context));
			minimum = std::min(minimum, elapsed);
			maximum = std::max(maximum, elapsed);
			total += elapsed;
			over10ms += elapsed > 10;
			++measured;
		}

		report.addUnsigned("latency", "samples", measured, "count", "sleep requested=1ms");
		if (measured) {
			report.addUnsigned("latency", "min", minimum, "ms");
			report.addUnsigned("latency", "max", maximum, "ms");
			report.addUnsigned("latency", "mean", total * 1000U / measured, "us", "quantized by 1ms clock");
			report.addUnsigned("latency", "over-10ms", over10ms, "count");
		}
	}

	void runSleepBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
		constexpr std::uint32_t requestedMs = 20;
		auto const start = uptimeMs(context);
		sleepMs(context, requestedMs);
		auto const elapsed = elapsedMs(start, uptimeMs(context));

		report.addUnsigned("scheduler", "sleep-20ms", elapsed, "ms", "measured wall time");
	}

	std::uint32_t runAddOperationBatches(std::uint32_t batches) {
		std::uint32_t value = 0x13579BDFU;
		std::uint32_t volatile sink = 0;
		for (std::uint32_t batch = 0; batch < batches; ++batch) {
			for (std::uint32_t index = 0; index < microOperationCount; ++index) {
				value += index + batch + 17U;
			}

			sink ^= value;
		}

		return sink;
	}

	std::uint32_t runSubOperationBatches(std::uint32_t batches) {
		std::uint32_t value = 0xFDB97531U;
		std::uint32_t volatile sink = 0;
		for (std::uint32_t batch = 0; batch < batches; ++batch) {
			for (std::uint32_t index = 0; index < microOperationCount; ++index) {
				value -= index + batch + 23U;
			}

			sink ^= value;
		}

		return sink;
	}

	std::uint32_t runMulOperationBatches(std::uint32_t batches) {
		std::uint32_t value = 0x10203040U;
		std::uint32_t volatile sink = 0;
		for (std::uint32_t batch = 0; batch < batches; ++batch) {
			for (std::uint32_t index = 0; index < microOperationCount; ++index) {
				value = value * 1664525U + index + batch + 1013904223U;
			}

			sink ^= value;
		}

		return sink;
	}

	std::uint32_t runDivOperationBatches(std::uint32_t batches) {
		std::uint32_t value = 0x89ABCDEFU;
		std::uint32_t volatile sink = 0;
		for (std::uint32_t batch = 0; batch < batches; ++batch) {
			for (std::uint32_t index = 0; index < microOperationCount; ++index) {
				auto const divisor = (index & 0x0FU) + 1U;
				value = (value + index + batch + 0x9E3779B9U) / divisor;
				value ^= divisor * 2654435761U;
			}

			sink ^= value;
		}

		return sink;
	}

	std::uint32_t microOperationBatchesFor(std::uint32_t iterations) {
		return std::min<std::uint32_t>(
		    std::max<std::uint32_t>(iterations / 1000U, minMicroOperationBatches),
		    maxMicroOperationBatches);
	}

	std::uint32_t piDigitsFor(std::uint32_t iterations) {
		return std::min<std::uint32_t>(
		    std::max<std::uint32_t>(iterations / 3000U, 16U),
		    piBenchmarkDigits);
	}

	std::uint32_t runPiSpigot(BenchmarkWorkspace& workspace, std::uint32_t digits) {
		auto const workSize = std::min<std::size_t>(digits * 10U / 3U + 1U, workspace.piWork.size());
		for (std::size_t index = 0; index < workSize; ++index) {
			workspace.piWork[index] = 2U;
		}

		std::uint32_t checksum = 0;
		std::uint32_t nines = 0;
		std::uint32_t predigit = 0;
		for (std::uint32_t digit = 0; digit < digits; ++digit) {
			std::uint32_t carry = 0;
			for (std::size_t reverse = workSize; reverse > 0; --reverse) {
				auto const index = reverse - 1U;
				auto const position = static_cast<std::uint32_t>(reverse);
				auto const value = static_cast<std::uint64_t>(workspace.piWork[index]) * 10ULL
				    + static_cast<std::uint64_t>(carry) * position;
				auto const denominator = position * 2U - 1U;
				workspace.piWork[index] = static_cast<std::uint32_t>(value % denominator);
				carry = static_cast<std::uint32_t>(value / denominator);
			}

			workspace.piWork[0] = carry % 10U;
			carry /= 10U;
			if (carry == 9U) {
				++nines;
			} else if (carry == 10U) {
				checksum = checksum * 33U + predigit + 1U;
				while (nines-- != 0U) {
					checksum = checksum * 33U;
				}

				predigit = 0;
				nines = 0;
			} else {
				checksum = checksum * 33U + predigit;
				predigit = carry;
				while (nines-- != 0U) {
					checksum = checksum * 33U + 9U;
				}

				nines = 0;
			}
		}

		return checksum ^ predigit ^ (nines << 16U);
	}

	void addMicroOperationRow(BenchmarkReport& report,
	    std::string_view metric,
	    std::uint32_t elapsed,
	    std::uint32_t batches,
	    std::uint32_t checksum) {
		FixedText note;
		note.append("batches=");
		note.appendUnsigned(batches);
		note.append(" checksum=");
		note.appendUnsigned(checksum);
		auto const usPerThousand = (static_cast<std::uint64_t>(elapsed) * 1000ULL + batches / 2U) / nonZeroElapsed(batches);
		report.addUnsigned("math", metric, usPerThousand, "us/1k", note.view());
	}

	void runArithmeticMicroBenchmark(lexxos::os::AppContext& context,
	    BenchmarkReport& report,
	    BenchmarkWorkspace& workspace,
	    std::uint32_t iterations) {
		auto const batches = microOperationBatchesFor(iterations);
		auto start = uptimeMs(context);
		auto checksum = runAddOperationBatches(batches);
		addMicroOperationRow(report, "add-1000", elapsedMs(start, uptimeMs(context)), batches, checksum);

		start = uptimeMs(context);
		checksum = runSubOperationBatches(batches);
		addMicroOperationRow(report, "sub-1000", elapsedMs(start, uptimeMs(context)), batches, checksum);

		start = uptimeMs(context);
		checksum = runMulOperationBatches(batches);
		addMicroOperationRow(report, "mul-1000", elapsedMs(start, uptimeMs(context)), batches, checksum);

		start = uptimeMs(context);
		checksum = runDivOperationBatches(batches);
		addMicroOperationRow(report, "div-1000", elapsedMs(start, uptimeMs(context)), batches, checksum);

		auto const digits = piDigitsFor(iterations);
		start = uptimeMs(context);
		checksum = runPiSpigot(workspace, digits);
		FixedText note;
		note.append("digits=");
		note.appendUnsigned(digits);
		note.append(" checksum=");
		note.appendUnsigned(checksum);
		report.addUnsigned("math", "pi-spigot", elapsedMs(start, uptimeMs(context)), "ms", note.view());
	}

#if defined(LEXXOS_KERNEL_FREERTOS)
	struct PingPongState {
		SemaphoreHandle_t toWorker{nullptr};
		SemaphoreHandle_t toMain{nullptr};
		std::uint32_t rounds{0};
		std::atomic<std::uint32_t> completed{0};
		std::uint32_t checksum{0};
	};

	void pingPongWorkerEntry(void* userData) {
		auto& state = *static_cast<PingPongState*>(userData);
		std::uint32_t checksum = 0;
		for (std::uint32_t round = 0; round < state.rounds; ++round) {
			(void) xSemaphoreTake(state.toWorker, portMAX_DELAY);
			checksum = checksum * 33U + round;
			(void) xSemaphoreGive(state.toMain);
		}

		state.checksum = checksum;
		state.completed.store(1U, std::memory_order_release);
	}

	void rtosTickProbeEntry(void* userData) {
		auto& state = *static_cast<RtosTickProbeState*>(userData);
		auto const start = state.uptimeMs != nullptr ? state.uptimeMs() : 0U;
		for (std::uint32_t pass = 0; pass < state.passes; ++pass) {
			lexxos::kernel::sleepMs(state.sleepMs);
		}

		auto const end = state.uptimeMs != nullptr ? state.uptimeMs() : start;
		state.elapsed = elapsedMs(start, end);
		state.completed.store(true, std::memory_order_release);
	}
#endif

	void addRtosTickProbe(lexxos::os::AppContext& context, BenchmarkReport& report, std::string_view metric, int coreAffinity) {
#if defined(LEXXOS_KERNEL_FREERTOS)
		RtosTickProbeState state{
		    .uptimeMs = context.platform().clock.uptimeMs,
		    .sleepMs = rtosTickProbeSleepMs,
		    .passes = rtosTickProbePasses,
		};
		if (state.uptimeMs == nullptr) {
			report.add("rtos", metric, "n/a", "", "uptime unavailable");
			return;
		}

		if (!lexxos::kernel::createTask(rtosTickProbeEntry,
		        &state,
		        {
		            .name = "bench-tick",
		            .stackSizeBytes = 1024,
		            .priority = 1,
		            .executionClass = lexxos::kernel::TaskExecutionClass::Background,
		            .coreAffinity = coreAffinity,
		        })) {
			report.add("rtos", metric, "failed", "", "createTask");
			return;
		}

		while (!state.completed.load(std::memory_order_acquire)) {
			lexxos::kernel::sleepMs(1);
		}

		auto const expected = rtosTickProbeSleepMs * rtosTickProbePasses;
		auto const drift = state.elapsed > expected ? state.elapsed - expected : expected - state.elapsed;
		FixedText note;
		note.append("target=");
		note.appendUnsigned(expected);
		note.append(" ");
		appendCoreNote(note, coreAffinity);
		report.addUnsigned("rtos", metric, state.elapsed, "ms", note.view());

		FixedText driftMetric;
		driftMetric.append(metric);
		driftMetric.append("-drift");
		report.addUnsigned("rtos", driftMetric.view(), drift, "ms");
#else
		(void) context;
		(void) coreAffinity;
		report.add("rtos", metric, "n/a", "", "not FreeRTOS");
#endif
	}

	void runRtosTickBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
		addRtosTickProbe(context, report, "tick-core0", 0);
		if (schedulerCoreCount() > 1) {
			addRtosTickProbe(context, report, "tick-core1", 1);
		} else {
			report.add("rtos", "tick-core1", "n/a", "", "single-core scheduler");
		}
	}

#if defined(LEXXOS_KERNEL_FREERTOS)
	struct SyncSmokeState {
		lexxos::devices::BusTransactionQueue queue;
		lexxos::kernel::Event completed;
		bool acquired{false};
		bool poisoned{false};
	};

	// Lazy placement construction avoids function-local static guard atomics
	// on MMU-off RK3506 while keeping the task context alive after timeout.
	alignas(SyncSmokeState) std::byte g_syncSmokeStorage[sizeof(SyncSmokeState)]{};
	SyncSmokeState* g_syncSmokeState{nullptr};

	void syncSmokeWorker(void* userData) {
		auto& state = *static_cast<SyncSmokeState*>(userData);
		state.acquired = state.queue.acquire(lexxos::devices::DeviceResourcePriority::System, 2000);
		if (state.acquired) {
			state.queue.release();
		}

		(void) state.completed.signal();
	}

	struct QueuePriorityState;

	struct QueuePriorityWorker {
		QueuePriorityState* state{nullptr};
		lexxos::devices::DeviceResourcePriority priority{lexxos::devices::DeviceResourcePriority::Normal};
		std::uint8_t id{0};
		std::uint32_t timeoutMs{2000};
		bool acquired{false};
	};

	struct QueuePriorityState {
		lexxos::devices::BusTransactionQueue queue;
		lexxos::kernel::Semaphore completed{3, 0};
		std::array<QueuePriorityWorker, 3> workers{};
		std::array<std::uint8_t, 3> order{};
		std::uint32_t orderCount{0};
		bool poisoned{false};
	};

	// Keep worker contexts alive even when a timed-out probe is broken.
	alignas(QueuePriorityState) std::byte g_queuePriorityStorage[sizeof(QueuePriorityState)]{};
	QueuePriorityState* g_queuePriorityState{nullptr};

	void queuePriorityWorker(void* userData) {
		auto& worker = *static_cast<QueuePriorityWorker*>(userData);
		auto& state = *worker.state;
		worker.acquired = state.queue.acquire(worker.priority, worker.timeoutMs);
		if (worker.acquired) {
			// Only the current bus owner writes to the order array.
			if (state.orderCount < state.order.size()) {
				state.order[state.orderCount++] = worker.id;
			}

			state.queue.release();
		}

		(void) state.completed.release();
	}

	void runQueuePrioritySmoke(BenchmarkReport& report) {
		using lexxos::devices::DeviceResourcePriority;
		if (g_queuePriorityState == nullptr) {
			g_queuePriorityState = new (g_queuePriorityStorage) QueuePriorityState{};
		}

		auto& state = *g_queuePriorityState;
		if (state.poisoned || !state.completed.valid() || state.queue.busy() || state.queue.pending() != 0) {
			report.add("sync", "bus-priority", "failed", "", "probe unavailable");
			return;
		}

		state.orderCount = 0;
		for (auto& worker : state.workers) {
			worker.acquired = false;
		}

		if (!state.queue.acquire(DeviceResourcePriority::Normal, 0)) {
			report.add("sync", "bus-priority", "failed", "", "initial owner");
			return;
		}

		// Enqueue deterministically: System A, Background, System B.
		// The expected grant order is A, B, Background (priority + FIFO).
		constexpr std::array priorities{
		    DeviceResourcePriority::System,
		    DeviceResourcePriority::Background,
		    DeviceResourcePriority::System,
		};
		std::uint32_t created = 0;
		bool enqueued = true;
		for (std::uint32_t index = 0; index < state.workers.size(); ++index) {
			auto& worker = state.workers[index];
			worker.state = &state;
			worker.priority = priorities[index];
			worker.timeoutMs = 2000;
			worker.id = static_cast<std::uint8_t>(index);
			if (!lexxos::kernel::createTask(queuePriorityWorker, &worker,
			        {
			            .name = "bench-qwait",
			            .stackSizeBytes = 4096,
			            .priority = 2,
			            .executionClass = lexxos::kernel::TaskExecutionClass::Background,
			        })) {
				enqueued = false;
				break;
			}
			++created;
			bool queued = false;
			for (std::uint32_t elapsed = 0; elapsed < 500; ++elapsed) {
				if (state.queue.pending() == created) {
					queued = true;
					break;
				}

				lexxos::kernel::sleepMs(1);
			}

			if (!queued) {
				enqueued = false;
				break;
			}
		}

		state.queue.release();
		bool completed = true;
		for (std::uint32_t index = 0; index < created; ++index) {
			if (!state.completed.acquire(2500)) {
				completed = false;
				break;
			}
		}

		state.poisoned = !completed;
		bool const priorityOk = enqueued && completed && created == 3 && state.orderCount == 3 && state.order[0] == 0 && state.order[1] == 2 && state.order[2] == 1 && state.workers[0].acquired && state.workers[1].acquired && state.workers[2].acquired && !state.queue.busy() && state.queue.pending() == 0;
		FixedText note;
		note.append("expected=0,2,1 actual=");
		for (std::uint32_t index = 0; index < state.orderCount; ++index) {
			if (index != 0) {
				note.append(",");
			}

			note.appendUnsigned(state.order[index]);
		}

		report.add("sync", "bus-priority", priorityOk ? "passed" : "failed", "", note.view());
	}

	void runQueueTimeoutRaceSmoke(BenchmarkReport& report) {
		using lexxos::devices::DeviceResourcePriority;
		if (g_queuePriorityState == nullptr) {
			report.add("sync", "bus-race", "failed", "", "priority probe not initialized");
			return;
		}

		auto& state = *g_queuePriorityState;
		if (state.poisoned || !state.completed.valid() || state.queue.busy() || state.queue.pending() != 0) {
			report.add("sync", "bus-race", "failed", "", "probe unavailable");
			return;
		}

		// Release just before, during and after the waiter's timeout.
		// The queue must always end empty regardless of which side wins.
		constexpr std::array<std::uint32_t, 8> releaseDelaysMs{0, 10, 18, 19, 20, 21, 25, 35};
		constexpr std::uint32_t timeoutMs = 20;
		std::uint32_t completedRounds = 0;
		std::uint32_t granted = 0;
		std::uint32_t timedOut = 0;
		bool healthy = true;
		for (auto const delayMs : releaseDelaysMs) {
			// The previous worker may have posted completion but not yet
			// reached the trampoline's final suspension point.
			lexxos::kernel::sleepMs(1);
			(void) lexxos::kernel::reapFinishedTasks();

			auto& worker = state.workers[0];
			worker.state = &state;
			worker.priority = DeviceResourcePriority::System;
			worker.timeoutMs = timeoutMs;
			worker.id = 0;
			worker.acquired = false;
			if (!state.queue.acquire(DeviceResourcePriority::Normal, 0)) {
				healthy = false;
				break;
			}

			if (!lexxos::kernel::createTask(queuePriorityWorker, &worker,
			        {
			            .name = "bench-qrace",
			            .stackSizeBytes = 4096,
			            .priority = 2,
			            .executionClass = lexxos::kernel::TaskExecutionClass::Background,
			        })) {
				state.queue.release();
				healthy = false;
				break;
			}

			bool queued = false;
			for (std::uint32_t attempt = 0; attempt < 100; ++attempt) {
				if (state.queue.pending() == 1) {
					queued = true;
					break;
				}

				lexxos::kernel::sleepMs(1);
			}

			if (queued && delayMs != 0) {
				lexxos::kernel::sleepMs(delayMs);
			}

			state.queue.release();

			if (!state.completed.acquire(2500)) {
				// Worker might still hold its pointer into persistent state.
				state.poisoned = true;
				healthy = false;
				break;
			}

			if (worker.acquired) {
				++granted;
			} else {
				++timedOut;
			}

			++completedRounds;

			if (!queued || state.queue.busy() || state.queue.pending() != 0) {
				healthy = false;
				break;
			}
		}

		FixedText note;
		note.append("rounds=");
		note.appendUnsigned(completedRounds);
		note.append(" granted=");
		note.appendUnsigned(granted);
		note.append(" timedout=");
		note.appendUnsigned(timedOut);
		bool const passed = healthy && completedRounds == releaseDelaysMs.size() && granted != 0 && timedOut != 0;
		report.add("sync", "bus-race", passed ? "passed" : "failed", "", note.view());
	}

	#if defined(LEXXOS_PLATFORM_RK3506) && defined(LEXXOS_RK3506_FREERTOS)
	void runTickIrqSmoke(BenchmarkReport& report) {
		lexxos::kernel::Event event;
		if (!event.valid() || !lexxos::platform::rk3506::armTickEventProbe(event)) {
			report.add("irq", "tick-from-isr", "failed", "", "probe arm");
			return;
		}

		// PPI27 is already owned by the FreeRTOS tick. No new IRQ line or DMA.
		bool const awakened = event.wait(100);
		auto const notifications = lexxos::platform::rk3506::disarmTickEventProbe(event);
		FixedText note;
		note.append("PPI27 give-from-isr=");
		note.appendUnsigned(notifications);
		report.add("irq", "tick-from-isr", awakened && notifications != 0 ? "passed" : "failed", "", note.view());
	}
	#endif
#endif

	void runKernelSyncSmoke(BenchmarkReport& report) {
#if defined(LEXXOS_KERNEL_FREERTOS)
		lexxos::kernel::Event event;
		bool const eventOk = event.valid() && !event.wait(0) && event.signal() && !event.signal() && event.wait(0) && !event.wait(0) && !event.wait(2);
		report.add("sync", "event", eventOk ? "passed" : "failed", "", "binary, auto-reset, timeout");

		lexxos::kernel::Mutex mutex;
		bool const mutexOk = mutex.valid() && mutex.lock(0);
		if (mutexOk) {
			mutex.unlock();
		}

		report.add("sync", "mutex", mutexOk ? "passed" : "failed", "", "lock/unlock");

		lexxos::kernel::Semaphore semaphore{2, 1};
		bool const semaphoreOk = semaphore.valid() && semaphore.acquire(0) && !semaphore.acquire(0) && semaphore.release() && semaphore.release() && !semaphore.release() && semaphore.acquire(0) && semaphore.acquire(0) && !semaphore.acquire(0);
		report.add("sync", "semaphore", semaphoreOk ? "passed" : "failed", "", "count, bounds");

		// Persistent state prevents stack-use-after-free if a broken backend
		// fails to wake the worker before the deadline. A failed run is not
		// restarted until reboot; the old task may still reference the state.
		if (g_syncSmokeState == nullptr) {
			g_syncSmokeState = new (g_syncSmokeStorage) SyncSmokeState{};
		}

		auto& state = *g_syncSmokeState;
		if (state.poisoned || !state.completed.valid() || state.queue.busy() || state.queue.pending() != 0) {
			report.add("sync", "bus-event", "failed", "", "probe unavailable");
			return;
		}

		(void) state.completed.wait(0);

		state.acquired = false;
		if (!state.queue.acquire(lexxos::devices::DeviceResourcePriority::Normal, 0)) {
			report.add("sync", "bus-event", "failed", "", "initial owner");
			return;
		}

		if (!lexxos::kernel::createTask(syncSmokeWorker, &state,
		        {
		            .name = "bench-sync",
		            .stackSizeBytes = 4096,
		            .priority = 2,
		            .executionClass = lexxos::kernel::TaskExecutionClass::Background,
		        })) {
			state.queue.release();
			report.add("sync", "bus-event", "failed", "", "createTask");
			return;
		}

		bool queued = false;
		for (std::uint32_t elapsed = 0; elapsed < 500; ++elapsed) {
			if (state.queue.pending() == 1) {
				queued = true;
				break;
			}

			lexxos::kernel::sleepMs(1);
		}

		state.queue.release();
		bool const completed = state.completed.wait(2500);
		state.poisoned = !completed;
		bool const busOk = completed && queued && state.acquired && !state.queue.busy() && state.queue.pending() == 0;
		report.add("sync", "bus-event", busOk ? "passed" : "failed", "",
		    "task wakeup, queue handoff, bounded wait");
		runQueuePrioritySmoke(report);
		runQueueTimeoutRaceSmoke(report);

	#if defined(LEXXOS_PLATFORM_RK3506) && defined(LEXXOS_RK3506_FREERTOS)
		runTickIrqSmoke(report);
	#endif
#else
		report.add("sync", "primitives", "n/a", "", "requires FreeRTOS backend");
#endif
	}

	void runRtosPrimitiveBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
#if defined(LEXXOS_KERNEL_FREERTOS)
		auto* mutex = xSemaphoreCreateMutex();
		if (mutex == nullptr) {
			report.add("rtos", "mutex", "failed", "", "xSemaphoreCreateMutex");
		} else {
			auto const start = uptimeMs(context);
			std::uint32_t checksum = 0;
			for (std::uint32_t index = 0; index < lockBenchmarkIterations; ++index) {
				(void) xSemaphoreTake(mutex, portMAX_DELAY);
				checksum = checksum * 33U + index;
				(void) xSemaphoreGive(mutex);
			}

			auto const elapsed = elapsedMs(start, uptimeMs(context));
			vSemaphoreDelete(mutex);
			FixedText note;
			note.append("take+give checksum=");
			note.appendUnsigned(checksum);
			report.addUnsigned("rtos", "mutex-10k", elapsed, "ms", note.view());
			report.addUnsigned("rtos", "mutex-rate", perSecond(lockBenchmarkIterations * 2ULL, elapsed), "ops/s");
		}

		PingPongState state{};
		state.rounds = pingPongRounds;
		state.toWorker = xSemaphoreCreateBinary();
		state.toMain = xSemaphoreCreateBinary();
		if (state.toWorker == nullptr || state.toMain == nullptr) {
			report.add("rtos", "sem-pingpong", "failed", "", "xSemaphoreCreateBinary");
		} else if (!lexxos::kernel::createTask(pingPongWorkerEntry,
		               &state,
		               {
		                   .name = "bench-pingpong",
		                   .stackSizeBytes = 1024,
		                   .priority = 2,
		                   .executionClass = lexxos::kernel::TaskExecutionClass::Background,
		                   .coreAffinity = lexxos::kernel::anyTaskCore,
		               })) {
			report.add("rtos", "sem-pingpong", "failed", "", "createTask");
		} else {
			auto const start = uptimeMs(context);
			for (std::uint32_t round = 0; round < pingPongRounds; ++round) {
				(void) xSemaphoreGive(state.toWorker);
				(void) xSemaphoreTake(state.toMain, portMAX_DELAY);
			}

			while (state.completed.load(std::memory_order_acquire) == 0U) {
				sleepMs(context, 1);
			}

			auto const elapsed = elapsedMs(start, uptimeMs(context));
			FixedText note;
			note.append("rounds=");
			note.appendUnsigned(pingPongRounds);
			note.append(" checksum=");
			note.appendUnsigned(state.checksum);
			report.addUnsigned("rtos", "sem-pingpong", elapsed, "ms", note.view());
			report.addUnsigned("rtos", "sem-switch-rate", perSecond(pingPongRounds * 2ULL, elapsed), "ops/s");
		}

		if (state.toWorker != nullptr) {
			vSemaphoreDelete(state.toWorker);
		}

		if (state.toMain != nullptr) {
			vSemaphoreDelete(state.toMain);
		}
#else
		(void) context;
		report.add("rtos", "primitives", "n/a", "", "not FreeRTOS");
#endif
	}

	void runConsoleRendererStatsBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
		if (!lexxos::app::picocalc_lcd_console::isReady()) {
			report.add("render", "stats", "n/a", "", "lcd console not ready");
			return;
		}

		lexxos::app::picocalc_lcd_console::resetRenderStats();
		auto const start = uptimeMs(context);
		for (std::uint32_t line = 0; line < 16U; ++line) {
			FixedText text;
			text.append("bench-render line=");
			text.appendUnsigned(line);
			text.append(" abcdefghijklmnopqrstuvwxyz 0123456789\r\n");
			lexxos::app::console_mirror::write(text.view());
		}

		for (std::uint32_t pass = 0; pass < 8U; ++pass) {
			lexxos::app::console_mirror::flush();
		}

		auto const elapsed = elapsedMs(start, uptimeMs(context));
		auto const stats = lexxos::app::picocalc_lcd_console::renderStats();
		auto const info = lexxos::app::picocalc_lcd_console::info();

		report.addUnsigned("render", "diag-total", elapsed, "ms", "16 lines + 8 flushes");
		report.addUnsigned("render", "flush-calls", stats.flushCalls, "count");
		report.addUnsigned("render", "chars", stats.charactersProcessed, "count");
		report.addUnsigned("render", "draw-rows", stats.drawRowCalls, "count");
		report.addUnsigned("render", "dirty-rows", stats.dirtyRowsDrawn, "count");
		report.addUnsigned("render", "scroll-rows", stats.scrollRowsDrawn, "count");
		report.addUnsigned("render", "hw-scroll", stats.hardwareScrollAdvanceCalls, "count");
		report.addUnsigned("render", "hw-scroll-px", stats.hardwareScrollPixels, "px");
		report.addUnsigned("render", "preflush", stats.hardwareScrollPreFlushCalls, "count");
		report.add("render", "hw-enabled", info.hardwareScrollEnabled ? yesText : noText, "");
		report.addUnsigned("render", "hw-area", static_cast<std::uint32_t>(info.scrollAreaPixels), "px");
		report.addUnsigned("render", "hw-start", static_cast<std::uint32_t>(info.scrollStartPixels), "px");
		report.addUnsigned("render", "status-draws", stats.statusBarDraws, "count");
		report.addUnsigned("render", "begin-fails", stats.beginFrameFailures, "count");
		report.addUnsigned("render", "lock-fails", stats.displayLockFailures, "count");
		report.addUnsigned("render", "hw-config", stats.hardwareScrollConfigureCalls, "count");
		report.addUnsigned("render", "hw-config-fail", stats.hardwareScrollConfigureFailures, "count");
#else
		(void) context;
		report.add("render", "stats", "n/a", "", "display console disabled");
#endif
	}

	void runMultitaskBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace,
	    std::uint32_t iterations, std::uint32_t requestedWorkers, std::string_view metricPrefix, MultitaskPlacement placement) {
		workspace.workersStart.store(false, std::memory_order_release);

		std::uint32_t createdWorkers = 0;
		std::uint32_t const workerIterations = std::max<std::uint32_t>(iterations / requestedWorkers, minIterations);
		for (std::uint32_t index = 0; index < requestedWorkers; ++index) {
			auto& worker = workspace.workers[index];
			worker.start = &workspace.workersStart;
			worker.completed.store(false, std::memory_order_release);
			worker.iterations = workerIterations;
			worker.checksum = 0;
			auto const coreAffinity = multitaskWorkerCore(placement, index);

			if (!lexxos::kernel::createTask(benchmarkWorkerEntry,
			        &worker,
			        {
			            .name = "bench-worker",
			            .stackSizeBytes = 1024,
			            .priority = 1,
			            .executionClass = lexxos::kernel::TaskExecutionClass::Background,
			            .coreAffinity = coreAffinity,
			        })) {
				break;
			}
			++createdWorkers;
		}

		if (createdWorkers == 0) {
			report.add("multitask", "workers", "failed", "", "no free scheduler task slots");
			return;
		}

		auto const start = uptimeMs(context);
		workspace.workersStart.store(true, std::memory_order_release);
		for (std::uint32_t index = 0; index < createdWorkers; ++index) {
			while (!workspace.workers[index].completed.load(std::memory_order_acquire)) {
				sleepMs(context, 1);
			}
		}

		// Let task trampolines release their scheduler slots before the next placement run reuses them.
		sleepMs(context, 1);
		auto const elapsed = elapsedMs(start, uptimeMs(context));

		FixedText note;
		note.append("workers=");
		note.appendUnsigned(createdWorkers);
		note.append(" per-worker=");
		note.appendUnsigned(workerIterations);
		note.append(" placement=");
		note.append(multitaskPlacementName(placement));

		FixedText loopMetric;
		FixedText rateMetric;
		if (metricPrefix.empty()) {
			loopMetric.append("worker-loop");
			rateMetric.append("aggregate-rate");
		} else {
			loopMetric.append(metricPrefix);
			loopMetric.append("-loop");
			rateMetric.append(metricPrefix);
			rateMetric.append("-rate");
		}

		report.addUnsigned("multitask", loopMetric.view(), elapsed, "ms", note.view());
		report.addUnsigned("multitask", rateMetric.view(), perSecond(static_cast<std::uint64_t>(createdWorkers) * workerIterations, elapsed), "iter/s");
	}

	void runMultitaskBenchmarks(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace,
	    std::uint32_t iterations, std::uint32_t workers) {
		runMultitaskBenchmark(context, report, workspace, iterations, workers, {}, MultitaskPlacement::Default);
		runMultitaskBenchmark(context, report, workspace, iterations, workers, "core0", MultitaskPlacement::Core0);
		if (schedulerCoreCount() > 1) {
			runMultitaskBenchmark(context, report, workspace, iterations, workers, "split", MultitaskPlacement::Split);
		} else {
			report.add("multitask", "split-loop", "n/a", "", "single-core scheduler");
		}
	}

	void fillIoPattern(std::array<char, ioChunkBytes>& buffer, std::uint32_t seed) {
		for (std::size_t index = 0; index < buffer.size(); ++index) {
			buffer[index] = static_cast<char>((index + seed * 31U) & 0x7FU);
		}
	}

	void runVfsIoBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace,
	    lexxos::program::ParsedArguments const& arguments) {
		copyText(workspace.ioPath, arguments.valueOr("io-path", defaultIoPath));

		lexxos::vfs::Stat existing{};
		if (lexxos::vfs::stat(textView(workspace.ioPath), existing)) {
			report.add("vfs", "write-open", "failed", "", "temporary path already exists");
			return;
		}

		auto const bytesToTest = textView(workspace.ioPath).starts_with("/tmp/")
		    ? std::min<std::uint32_t>(ioBenchmarkBytes, lexxos::runtime::standardRuntimeProfile.vfs.maxTmpFileSizeBytes)
		    : ioBenchmarkBytes;
		lexxos::vfs::FileHandle output;
		auto openStatus = lexxos::vfs::open(textView(workspace.ioPath), lexxos::vfs::OpenMode::CreateTruncateWrite, output);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			report.add("vfs", "write-open", "failed", "", lexxos::vfs::toString(openStatus));
			return;
		}

		auto const writeStart = uptimeMs(context);
		for (std::uint32_t offset = 0; offset < bytesToTest; offset += ioChunkBytes) {
			fillIoPattern(workspace.ioBuffer, offset / ioChunkBytes);
			auto const status = output.write(std::string_view{workspace.ioBuffer.data(), std::min<std::size_t>(workspace.ioBuffer.size(), bytesToTest - offset)});
			if (status != lexxos::vfs::WriteStatus::Ok) {
				output.close();
				report.add("vfs", "write", "failed", "", lexxos::vfs::toString(status));
				return;
			}
		}

		auto const finishStatus = output.finish();
		if (finishStatus != lexxos::vfs::WriteStatus::Ok) {
			report.add("vfs", "write-close", "failed", "", lexxos::vfs::toString(finishStatus));
			return;
		}

		auto const syncStatus = lexxos::vfs::sync();
		if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
			report.add("vfs", "sync", "failed", "", lexxos::vfs::toString(syncStatus));
			return;
		}

		auto const writeElapsed = elapsedMs(writeStart, uptimeMs(context));

		lexxos::vfs::FileHandle input;
		openStatus = lexxos::vfs::open(textView(workspace.ioPath), lexxos::vfs::OpenMode::Read, input);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			report.add("vfs", "read-open", "failed", "", lexxos::vfs::toString(openStatus));
			return;
		}

		std::uint32_t readBytes = 0;
		auto const readStart = uptimeMs(context);
		while (readBytes < bytesToTest) {
			std::size_t bytesRead = 0;
			auto const status = input.read(workspace.ioBuffer.data(), workspace.ioBuffer.size(), bytesRead);
			if (status != lexxos::vfs::ReadStatus::Ok || bytesRead == 0) {
				input.close();
				auto const note = status == lexxos::vfs::ReadStatus::Ok ? std::string_view{"short read"} : lexxos::vfs::toString(status);
				report.add("vfs", "read", "failed", "", note);
				return;
			}

			for (std::size_t i = 0; i < bytesRead; ++i) {
				auto const offset = readBytes + i;
				auto const expected = static_cast<char>(((offset % ioChunkBytes) + (offset / ioChunkBytes) * 31U) & 0x7FU);
				if (workspace.ioBuffer[i] != expected) {
					report.add("vfs", "verify", "failed", "", "pattern mismatch");
					return;
				}
			}

			readBytes += static_cast<std::uint32_t>(bytesRead);
		}

		input.close();
		auto const readElapsed = elapsedMs(readStart, uptimeMs(context));
		(void) lexxos::vfs::remove(textView(workspace.ioPath));

		report.addUnsigned("vfs", "bytes", bytesToTest, "bytes");
		report.addUnsigned("vfs", "write", writeElapsed, "ms", textView(workspace.ioPath));
		report.addUnsigned("vfs", "write-rate", kibPerSecond(bytesToTest, writeElapsed), "KiB/s");
		report.addUnsigned("vfs", "read", readElapsed, "ms", textView(workspace.ioPath));
		report.addUnsigned("vfs", "read-rate", perSecond(readBytes / 1024U, readElapsed), "KiB/s");
	}

	void runConsoleMirrorBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
#if FEATURE_ENABLED(DISPLAY)
		if (!lexxos::app::console_mirror::isReady()) {
			report.add("console", "mirror", "not-ready", "");
			return;
		}

		constexpr std::string_view line = "bench console/display pipeline sample line 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n";
		auto const bytes = static_cast<std::uint64_t>(line.size()) * consoleBenchmarkLines;

		auto const writeStart = uptimeMs(context);
		for (std::uint32_t index = 0; index < consoleBenchmarkLines; ++index) {
			lexxos::app::console_mirror::write(line);
		}

		auto const writeElapsed = elapsedMs(writeStart, uptimeMs(context));

		auto const flushStart = uptimeMs(context);
		for (std::uint32_t index = 0; index < consoleBenchmarkFlushPasses; ++index) {
			lexxos::app::console_mirror::flush();
		}

		auto const flushElapsed = elapsedMs(flushStart, uptimeMs(context));

		auto const refreshStart = uptimeMs(context);
		lexxos::app::console_mirror::refresh();
		auto const refreshElapsed = elapsedMs(refreshStart, uptimeMs(context));

		FixedText writeNote;
		writeNote.append("lines=");
		writeNote.appendUnsigned(consoleBenchmarkLines);
		report.addUnsigned("console", "mirror-write", writeElapsed, "ms", writeNote.view());
		report.addUnsigned("console", "mirror-rate", kibPerSecond(bytes, writeElapsed), "KiB/s");

		FixedText flushNote;
		flushNote.append("passes=");
		flushNote.appendUnsigned(consoleBenchmarkFlushPasses);
		report.addUnsigned("console", "mirror-flush", flushElapsed, "ms", flushNote.view());
		report.addUnsigned("console", "mirror-refresh", refreshElapsed, "ms");
#else
		(void) context;
		report.add("console", "mirror", "disabled", "", "display feature off");
#endif
	}

	void runConsoleClearBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report) {
#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
		if (!lexxos::app::console_mirror::isReady() || !lexxos::app::picocalc_lcd_console::isReady()) {
			report.add("console", "clear", "n/a", "", "lcd console not ready");
			return;
		}

		constexpr std::string_view line = "bench clear prefill line 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n";
		for (std::uint32_t index = 0; index < consoleClearBenchmarkFillLines; ++index) {
			lexxos::app::console_mirror::write(line);
		}

		for (std::uint32_t pass = 0; pass < consoleClearBenchmarkFlushPasses; ++pass) {
			lexxos::app::console_mirror::flush();
		}

		lexxos::app::picocalc_lcd_console::resetRenderStats();
		auto const clearStart = uptimeMs(context);
		lexxos::app::console_mirror::clear();
		auto const clearElapsed = elapsedMs(clearStart, uptimeMs(context));

		auto const flushStart = uptimeMs(context);
		for (std::uint32_t pass = 0; pass < consoleClearBenchmarkFlushPasses; ++pass) {
			lexxos::app::console_mirror::flush();
		}

		auto const flushElapsed = elapsedMs(flushStart, uptimeMs(context));
		auto const stats = lexxos::app::picocalc_lcd_console::renderStats();

		FixedText fillNote;
		fillNote.append("prefill-lines=");
		fillNote.appendUnsigned(consoleClearBenchmarkFillLines);
		report.addUnsigned("console", "clear", clearElapsed, "ms", fillNote.view());

		FixedText flushNote;
		flushNote.append("passes=");
		flushNote.appendUnsigned(consoleClearBenchmarkFlushPasses);
		report.addUnsigned("console", "clear-flush", flushElapsed, "ms", flushNote.view());
		report.addUnsigned("console", "clear-total", clearElapsed + flushElapsed, "ms");

		report.addUnsigned("render", "clear-calls", stats.clearCalls, "count");
		report.addUnsigned("render", "clear-fill", stats.clearTextAreaCalls, "count");
		report.addUnsigned("render", "clear-fill-fail", stats.clearTextAreaFailures, "count");
		report.addUnsigned("render", "clear-fill-px", stats.clearTextAreaPixels, "px");
		report.addUnsigned("render", "clear-draw-rows", stats.drawRowCalls, "count");
		report.addUnsigned("render", "clear-draw-cells", stats.drawCellCalls, "count");
		report.addUnsigned("render", "clear-lock-fails", stats.displayLockFailures, "count");
#else
		(void) context;
		report.add("console", "clear", "n/a", "", "display console disabled");
#endif
	}

#if FEATURE_ENABLED(DISPLAY)
	lexxos::devices::DeviceResourceLock lockDisplayForBenchmark(lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display) {
		for (std::uint32_t attempt = 0; attempt < 20; ++attempt) {
			auto lock = context.devices().lockDisplayDevice(
			    display.name(),
			    lexxos::devices::DeviceResourcePriority::Critical,
			    displayBenchmarkOwner);
			if (lock) {
				return lock;
			}

			sleepMs(context, 1);
		}

		return {};
	}

	void fillDisplayBenchmarkPixels(std::array<std::uint16_t, displayBenchmarkMaxWidth>& pixels,
	    std::uint32_t width,
	    std::uint32_t row) {
		for (std::uint32_t x = 0; x < width; ++x) {
			auto const band = (x / 16U + row) % 3U;
			pixels[x] = band == 0 ? displayBenchmarkColorA : band == 1 ? displayBenchmarkColorB
			                                                           : displayBenchmarkColorC;
		}
	}

	void fillDisplayBenchmarkRgb888Row(std::array<std::uint8_t, displayBenchmarkMaxWidth * rgb888BytesPerPixel>& pixels,
	    std::uint32_t width) {
		auto const bytes = std::min<std::size_t>(pixels.size(), static_cast<std::size_t>(width) * rgb888BytesPerPixel);
		for (std::size_t index = 0; index < bytes; ++index) {
			pixels[index] = 0;
		}
	}

	void runDirectDisplayBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace) {
		auto& devices = context.devices();
		auto* display = devices.primaryDisplayDevice();
		if (display == nullptr) {
			report.add("display", "device", "none", "");
			return;
		}

		if (!display->isReady()) {
			report.add("display", "device", "not-ready", "", display->name());
			return;
		}

		// Stop the text mirror while the raw-display probe owns the LCD. Otherwise
		// the background mirror task competes for the same SPI display and makes the
		// direct-device numbers noisy.
		lexxos::app::ScopedConsoleMirrorMode mirrorMode{lexxos::app::ConsoleMirrorMode::Graphics};
		sleepMs(context, 2);

		auto lock = lockDisplayForBenchmark(context, *display);
		if (!lock) {
			mirrorMode.restore();
			report.add("display", "lock", "failed", "", display->name());
			return;
		}

		auto const width = std::min<std::uint32_t>(
		    static_cast<std::uint32_t>(std::max(display->width(), 0)),
		    displayBenchmarkMaxWidth);
		auto const rows = std::min<std::uint32_t>(
		    static_cast<std::uint32_t>(std::max(display->height(), 0)),
		    displayBenchmarkRows);
		if (width == 0 || rows == 0) {
			mirrorMode.restore();
			report.add("display", "geometry", "invalid", "", display->name());
			return;
		}

		auto const start = uptimeMs(context);
		bool ok = display->beginFrame();
		for (std::uint32_t row = 0; ok && row < rows; ++row) {
			fillDisplayBenchmarkPixels(workspace.displayPixels, width, row);
			ok = display->blitRgb565({
			                             .x = 0,
			                             .y = static_cast<int>(row),
			                             .width = static_cast<int>(width),
			                             .height = 1,
			                         },
			    workspace.displayPixels.data(), static_cast<int>(width));
		}

		display->endFrame();
		auto const elapsed = elapsedMs(start, uptimeMs(context));
		auto const bytes = static_cast<std::uint64_t>(width) * rows * 2ULL;

		mirrorMode.restore();
		lexxos::app::console_mirror::refresh();

		if (!ok) {
			report.add("display", "blit", "failed", "", display->name());
			return;
		}

		FixedText note;
		note.appendUnsigned(width);
		note.append("x");
		note.appendUnsigned(rows);
		note.append(" rgb565");
		report.addUnsigned("display", "raw-blit", elapsed, "ms", note.view());
		report.addUnsigned("display", "raw-rate", kibPerSecond(bytes, elapsed), "KiB/s", display->name());
	}

	void runDirectTextAreaFillBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace) {
		auto& devices = context.devices();
		auto* display = devices.primaryDisplayDevice();
		if (display == nullptr) {
			report.add("display", "fill-text", "none", "");
			return;
		}

		if (!display->isReady()) {
			report.add("display", "fill-text", "not-ready", "", display->name());
			return;
		}

		std::uint32_t top = 0;
	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
		if (lexxos::app::picocalc_lcd_console::isReady()) {
			auto const info = lexxos::app::picocalc_lcd_console::info();
			if (info.statusBarPixels > 0) {
				top = static_cast<std::uint32_t>(info.statusBarPixels);
			}
		}
	#endif

		lexxos::app::ScopedConsoleMirrorMode mirrorMode{lexxos::app::ConsoleMirrorMode::Graphics};
		sleepMs(context, 2);

		auto lock = lockDisplayForBenchmark(context, *display);
		if (!lock) {
			mirrorMode.restore();
			report.add("display", "fill-text", "lock-failed", "", display->name());
			return;
		}

		auto const width = std::min<std::uint32_t>(
		    static_cast<std::uint32_t>(std::max(display->width(), 0)),
		    displayBenchmarkMaxWidth);
		auto const displayHeight = static_cast<std::uint32_t>(std::max(display->height(), 0));
		if (top >= displayHeight || width == 0) {
			mirrorMode.restore();
			report.add("display", "fill-text", "invalid", "", display->name());
			return;
		}

		auto const rows = displayHeight - top;
		auto const rect = lexxos::devices::Rect{
		    .x = 0,
		    .y = static_cast<int>(top),
		    .width = static_cast<int>(width),
		    .height = static_cast<int>(rows),
		};

		fillDisplayBenchmarkRgb888Row(workspace.displayRgb888Row, width);
		for (std::uint32_t index = 0; index < width; ++index) {
			workspace.displayPixels[index] = 0x0000U;
		}

		bool usedRgb888 = false;
		bool ok = false;
		auto const start = uptimeMs(context);
		if (display->beginFrame()) {
			if (display->beginRgb565Stream(rect)) {
				usedRgb888 = true;
				ok = true;
				for (std::uint32_t row = 0; row < rows; ++row) {
					if (!display->writeRgb888Stream(workspace.displayRgb888Row.data(), static_cast<int>(width))) {
						usedRgb888 = false;
						ok = false;
						break;
					}
				}

				display->endRgb565Stream();
			}

			if (!ok && display->beginRgb565Stream(rect)) {
				ok = true;
				for (std::uint32_t row = 0; row < rows; ++row) {
					if (!display->writeRgb565Stream(workspace.displayPixels.data(), static_cast<int>(width))) {
						ok = false;
						break;
					}
				}

				display->endRgb565Stream();
			}

			display->endFrame();
		}

		auto const elapsed = elapsedMs(start, uptimeMs(context));

		mirrorMode.restore();

		if (!ok) {
			report.add("display", "fill-text", "failed", "", display->name());
			return;
		}

		FixedText note;
		note.appendUnsigned(width);
		note.append("x");
		note.appendUnsigned(rows);
		note.append(usedRgb888 ? " rgb888" : " rgb565");
		auto const bytesPerPixel = usedRgb888 ? rgb888BytesPerPixel : 2U;
		auto const bytes = static_cast<std::uint64_t>(width) * rows * bytesPerPixel;
		report.addUnsigned("display", "fill-text", elapsed, "ms", note.view());
		report.addUnsigned("display", "fill-rate", kibPerSecond(bytes, elapsed), "KiB/s", display->name());
	}

#else
	void runDirectDisplayBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace) {
		(void) context;
		(void) workspace;
		report.add("display", "device", "disabled", "", "display feature off");
	}

	void runDirectTextAreaFillBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace) {
		(void) context;
		(void) workspace;
		report.add("display", "fill-text", "disabled", "", "display feature off");
	}
#endif

	bool openReportFile(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::array<char, lexxos::vfs::maxPathLength>& reportPath,
	    lexxos::vfs::FileHandle& file) {
		auto const requestedPath = arguments.valueOr("out", defaultReportPath);
		auto const defaultPathRequested = requestedPath.empty() || !arguments.has("out");
		if (defaultPathRequested) {
			buildTimestampedReportPath(context, arguments, defaultReportDirectory, reportPath);
			(void) lexxos::vfs::createDirectory(defaultReportDirectory);
		} else {
			copyText(reportPath, requestedPath);
		}

		lexxos::vfs::Stat existing{};
		if (lexxos::vfs::stat(textView(reportPath), existing)) {
			context.writeLine("bench: report path already exists; choose a new --out");
			return false;
		}

		auto status = lexxos::vfs::open(textView(reportPath), lexxos::vfs::OpenMode::CreateTruncateWrite, file);
		if (status == lexxos::vfs::OpenStatus::Ok) {
			return true;
		}

		context.write("bench: report save disabled, open failed: ");
		context.writeLine(lexxos::vfs::toString(status));
		return false;
	}

	void emitReport(lexxos::os::AppContext& context, BenchmarkReport const& report, lexxos::vfs::FileHandle*) {
		for (std::size_t i = 0; i < report.rowCount; ++i) {
			auto const& row = report.rows[i];
			std::array<char, 320> line{};
			std::size_t used = 0;
			for (auto field : {textView(row.section), textView(row.metric), textView(row.value), textView(row.unit), textView(row.note)}) {
				if (used) {
					appendToArray(line, used, "\t");
				}

				appendToArray(line, used, field);
			}
			appendToArray(line, used, "\n");
			context.write(std::string_view{line.data(), used});
		}
	}

	void announceStep(lexxos::os::AppContext& context, std::string_view step) {
		context.write("bench: ");
		context.writeLine(step);
	}

	// Profile-specific workspaces retain the same ProgramMemoryArena ownership.
	// Neither profile requires a second heap allocation after task creation.
	template <typename Workspace>
	Workspace* allocateWorkspace(lexxos::os::AppContext& context) {
		auto* arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("bench: program memory arena is unavailable");
			return nullptr;
		}

		auto* memory = arena->allocate(sizeof(Workspace), alignof(Workspace));
		if (memory == nullptr) {
			context.writeLine("bench: not enough program workspace memory");
			return nullptr;
		}

		return new (memory) Workspace{};
	}

	std::uint32_t benchmarkInternalMemoryQuota(lexxos::program::ProgramInvocation const& invocation) {
		std::string_view profile;
		for (int index = 1; index < invocation.argc; ++index) {
			std::string_view const argument{invocation.argv[index]};
			if (argument == "--profile" && index + 1 < invocation.argc) {
				profile = invocation.argv[++index];
			} else if (argument.starts_with("--profile=")) {
				profile = argument.substr(sizeof("--profile=") - 1U);
			}
		}

		if (profile == "sync") {
			return static_cast<std::uint32_t>(sizeof(SyncBenchmarkWorkspace));
		}

		if (profile == "vm" || profile == "vm-detail") {
			return static_cast<std::uint32_t>(sizeof(VmBenchmarkWorkspace));
		}

		return static_cast<std::uint32_t>(sizeof(BenchmarkWorkspace));
	}

} // namespace

ProgramDescriptor BenchmarkProgram::descriptor() {
	return {
	    .name = "bench",
	    .help = "run portable LexxOS system benchmark",
	    .description = "Quick, full, soak, sync and RV32I VM tests with incremental TSV reports, resource snapshots, display and verified SD reads. File writes require a new --write-dir.",
	    .usage = "bench [--profile quick|full|soak|sync|vm|vm-detail] [--vm-case substring] [--vm-repeat 1..9] [--duration seconds] [--device name] [--sd-start lba] [--sd-blocks count] [--write-dir NEW-path] [--file-bytes count] [--iterations count] [--tasks count] [--out path] [--io-path path] [--no-display] [--no-mirror] [--no-save]",
	    .arguments = argumentList(benchmarkArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 16384,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(BenchmarkWorkspace)),
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .internalMemoryQuotaResolver = benchmarkInternalMemoryQuota,
	};
}

int BenchmarkProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const profile = arguments.valueOr("profile", "quick");
	if (profile != "quick" && profile != "full" && profile != "soak" && profile != "sync" && profile != "vm" && profile != "vm-detail") {
		context.writeErrorLine("bench: profile must be quick, full, soak, sync, vm or vm-detail");
		return 2;
	}

	auto number = [&](std::string_view name, std::uint32_t fallback, std::uint32_t min, std::uint32_t max, std::uint32_t& output) {
		output = fallback;
		if (!arguments.has(name)) {
			return true;
		}

		if (!parseUnsigned32(arguments.valueOr(name, {}), output) || output < min || output > max) {
			context.write("bench: invalid ");
			context.writeLine(name);
			return false;
		}
		return true;
	};

	std::uint32_t iterations{}, workers{}, duration{}, blocks{}, firstBlock{}, fileBytes{}, vmRepeats{};
	if (!number("iterations", profile == "quick" ? 20000U : (profile == "vm" || profile == "vm-detail") ? 2000U
	                                                                                                    : defaultIterations,
	        minIterations, maxIterations, iterations)
	    || !number("vm-repeat", 1, 1, 9, vmRepeats)
	    || !number("vm-repeat", 1, 1, 9, vmRepeats)
	    || !number("tasks", defaultWorkers, 1, maxWorkers, workers)
	    || !number("duration", 300, 1, 86400, duration)
	    || !number("sd-blocks", profile == "quick" ? 16 : 128, 1, 1024, blocks)
	    || !number("sd-start", 0, 0, UINT32_MAX, firstBlock)
	    || !number("file-bytes", 1048576, 1, 16777216, fileBytes)) {
		return 2;
	}

	if (profile != "vm-detail"
	    && (arguments.has("vm-case") || arguments.has("vm-repeat"))) {
		context.writeErrorLine("bench: --vm-case and --vm-repeat require --profile vm-detail");
		return 2;
	}

	if ((arguments.has("duration") && profile != "soak")
	    || (arguments.has("file-bytes") && !arguments.has("write-dir"))) {
		context.writeErrorLine("bench: --duration requires soak; --file-bytes requires --write-dir");
		return 2;
	}

	for (auto name : {"out", "io-path", "write-dir"}) {
		if (arguments.has(name)) {
			auto const path = arguments.valueOr(name, {});
			if (path.empty() || path.size() >= lexxos::vfs::maxPathLength || path.front() != '/') {
				context.writeErrorLine("bench: paths must be absolute and fit VFS maxPathLength");
				return 2;
			}
		}
	}

	if (arguments.has("no-mirror") && !arguments.has("no-display")) {
		context.writeErrorLine("bench: --no-mirror requires --no-display");
		return 2;
	}

	std::optional<lexxos::app::ScopedConsoleMirrorSuspension> mirrorSuspension;
	if (arguments.has("no-mirror")) {
		mirrorSuspension.emplace();
	}

	auto const vmProfile = profile == "vm" || profile == "vm-detail";
	auto* workspace = profile != "sync" && !vmProfile ? allocateWorkspace<BenchmarkWorkspace>(context) : nullptr;
	auto* syncWorkspace = profile == "sync" ? allocateWorkspace<SyncBenchmarkWorkspace>(context) : nullptr;
	auto* vmWorkspace = vmProfile ? allocateWorkspace<VmBenchmarkWorkspace>(context) : nullptr;
	if (workspace == nullptr && syncWorkspace == nullptr && vmWorkspace == nullptr) {
		return 1;
	}

	auto& report = vmWorkspace != nullptr ? vmWorkspace->report : syncWorkspace != nullptr ? syncWorkspace->report
	                                                                                       : workspace->report;
	auto& reportPath = vmWorkspace != nullptr ? vmWorkspace->reportPath : syncWorkspace != nullptr ? syncWorkspace->reportPath
	                                                                                               : workspace->reportPath;
	lexxos::vfs::FileHandle reportFile;
	bool const saveRequested = !arguments.has("no-save");
	bool const saveOpened = saveRequested && openReportFile(context, arguments, reportPath, reportFile);
	ReportOutputStream output{context.standardOutput(), saveOpened ? &reportFile : nullptr};
	auto runContext = context.createChild({.standardOutput = &output, .standardError = &output});
	runContext.writeLine("LexxOS benchmark report v2");
	runContext.writeLine("section\tmetric\tvalue\tunit\tnote");
	runContext.write("report-path\t");
	runContext.writeLine(saveOpened ? textView(reportPath) : "console-only");
	unsigned failures = saveRequested && !saveOpened && arguments.has("out") ? 1 : 0;
	unsigned skipped{}, rounds{}, phases{};
	std::uint32_t reportOutputMs{}, phaseWorkMs{}, reportRows{};
	bool deadlineReached{};
	auto const started = uptimeMs(context);
	auto flush = [&] {
		auto const outputStarted = uptimeMs(context);
		reportRows += static_cast<std::uint32_t>(report.rowCount);
		emitReport(runContext, report, nullptr);
		failures += report.failures + report.dropped;
		skipped += report.skipped;
		if (report.dropped) {
			runContext.writeLine("bench: report-overflow");
		}

		report.rowCount = 0;
		report.failures = 0;
		report.skipped = 0;
		report.dropped = 0;
		if (reportFile.isOpen() && lexxos::vfs::sync() != lexxos::vfs::SyncStatus::Ok) {
			++failures;
		}

		reportOutputMs += elapsedMs(outputStarted, uptimeMs(context));
	};

	auto canRun = [&] {
		deadlineReached = profile == "soak" && elapsedMs(started, uptimeMs(context)) >= duration * 1000U;
		return !context.cancellationRequested() && !deadlineReached;
	};

	auto phase = [&](std::string_view name, auto work) {
		if (!canRun()) {
			return;
		}

		announceStep(runContext, name);
		report.add("phase", "name", name, "");
		detail::sampleBenchmarkResources(runContext, report);
		flush();
		auto const start = uptimeMs(context);
		work();
		++phases;
		auto const workMs = elapsedMs(start, uptimeMs(context));
		phaseWorkMs += workMs;
		report.add("phase", "name", name, "", "after");
		report.addUnsigned("phase", "elapsed", workMs, "ms", "excludes report output");
		detail::sampleBenchmarkResources(runContext, report);
		flush();
	};

	report.add("suite", "profile", profile, "");
	report.add("output", "lcd-mirror", arguments.has("no-mirror") ? "suspended" : "enabled", "", "--no-display only skips active display tests");
	report.add("output", "save", saveOpened ? "enabled" : "skipped", "", saveOpened ? "incremental report" : "console-only; no writable report target");
	report.add("suite", "clock-resolution", "1", "ms", "legacy rates clamp sub-ms elapsed to 1ms");
	report.addUnsigned("suite", "workspace",
	    profile == "sync" ? sizeof(SyncBenchmarkWorkspace)
	        : vmProfile   ? sizeof(VmBenchmarkWorkspace)
	                      : sizeof(BenchmarkWorkspace),
	    "bytes");
	report.addUnsigned("suite", "start-uptime", started, "ms");
	collectConfiguration(runContext, report);
	flush();
	do {
		++rounds;
		report.addUnsigned("suite", "round", rounds, "count");
		flush();

		if (profile == "sync") {
			phase("sync", [&] { runKernelSyncSmoke(report); });
			break;
		}

		if (vmProfile) {
			phase("cpu-native", [&] { runCpuBenchmark(runContext, report, iterations); });
			phase("rv32i", [&] { runVmBenchmark(runContext, report, *vmWorkspace, iterations); });

			if (profile == "vm-detail") {
				phase("rv32i-detail", [&] {
					runVmDetailedBenchmark(
					    runContext, report, *vmWorkspace, iterations,
					    arguments.valueOr("vm-case", {}), vmRepeats, flush);
				});
			}

			break;
		}

		phase("native-before", [&] { detail::sampleBenchmarkNative(report); });
		phase("devices-before", [&] { detail::sampleBenchmarkDevices(runContext, report); });
		phase("cpu", [&] { runCpuBenchmark(runContext, report, iterations); });
		phase("memory", [&] { runMemoryBenchmark(runContext, report, *workspace, iterations); });
		phase("scheduler", [&] {
			runYieldBenchmark(runContext, report, iterations);
			runSleepBenchmark(runContext, report);
			runLatencyBenchmark(runContext, report, profile == "quick" ? 16 : 128);
		});

		if (profile != "quick") {
			phase("math", [&] { runArithmeticMicroBenchmark(runContext, report, *workspace, iterations); });
			phase("rtos", [&] { runRtosTickBenchmark(runContext, report); runRtosPrimitiveBenchmark(runContext, report); });
			phase("multitask", [&] { runMultitaskBenchmarks(runContext, report, *workspace, iterations, workers); });
		}

		phase("vfs", [&] { runVfsIoBenchmark(runContext, report, *workspace, arguments); });
		phase("storage", [&] {
			detail::runBenchmarkStorage(runContext, report, workspace->probes, arguments.valueOr("device", {}),
			    firstBlock, blocks, arguments.valueOr("write-dir", {}), fileBytes);
		});

		if (!arguments.has("no-display")) {
			phase("console", [&] { runConsoleMirrorBenchmark(runContext, report); });
			phase("display", [&] {
				runDirectDisplayBenchmark(runContext, report, *workspace);
				if (profile != "quick") {
					runDirectTextAreaFillBenchmark(runContext, report, *workspace);
				}
			});

			phase("render", [&] {
				runConsoleRendererStatsBenchmark(runContext, report);
				if (profile != "quick") {
					runConsoleClearBenchmark(runContext, report);
				}
			});
		} else {
			report.add("display", "tests", "skipped", "", "--no-display");
			flush();
		}

		phase("native-after", [&] { detail::sampleBenchmarkNative(report); });
		phase("devices-after", [&] { detail::sampleBenchmarkDevices(runContext, report); });

		if (profile != "soak" || !canRun()) {
			break;
		}

		sleepMs(runContext, 10);
	} while (canRun());

	report.addUnsigned("summary", "phase-work", phaseWorkMs, "ms", "sum of phase wall times");
	report.addUnsigned("summary", "report-output", reportOutputMs, "ms", "writes + sync before summary; includes scheduling");
	report.addUnsigned("summary", "report-rows", reportRows, "count", "before summary");
	report.addUnsigned("summary", "rounds-started", rounds, "count");
	report.addUnsigned("summary", "phases-completed", phases, "count");
	report.add("summary", "duration-target-reached", deadlineReached ? "yes" : "no", "");
	report.addUnsigned("summary", "elapsed", elapsedMs(started, uptimeMs(context)), "ms", "includes output and report sync");
	report.addUnsigned("summary", "failures", failures, "count");
	report.addUnsigned("summary", "unavailable-or-skipped", skipped, "rows", "not passed tests");
	report.add("summary", "result", context.cancellationRequested() ? "cancelled" : failures || output.failed() ? "failed"
	                                                                                                            : "completed",
	    "");
	report.add("summary", "coverage", "see skipped rows", "", "completion is not full hardware acceptance");
	flush();

	auto const reportFinishStatus = reportFile.finish();
	if (reportFinishStatus != lexxos::vfs::WriteStatus::Ok) {
		context.writeError("bench: report close failed: ");
		context.writeErrorLine(lexxos::vfs::toString(reportFinishStatus));
		++failures;
	}

	if (output.failed()) {
		context.writeErrorLine("bench: report output failed");
	}

	return context.cancellationRequested() ? 130 : failures || output.failed() ? 1
	                                                                           : 0;
}

} // namespace lexxos::apps::system_programs
