#include <lexxos/apps/core/benchmark_program.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>

#include <lexxos/app/console_mirror.h>
#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
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
	constexpr char defaultReportPath[] = "/mnt/sd/apps/bench/system-benchmark.txt";
	constexpr char defaultReportDirectory[] = "/mnt/sd/apps/bench";
	constexpr char fallbackReportDirectory[] = "/tmp";
	constexpr char defaultIoPath[] = "/tmp/bench-io.tmp";

	constexpr ProgramArgumentSpec benchmarkArguments[] = {
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
         .help = "report file path, default: timestamped file in /mnt/sd/bench",
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
		std::array<char, 32> value{};
		std::array<char, 16> unit{};
		std::array<char, 48> note{};
	};

	struct BenchmarkReport {
		static constexpr std::size_t maxRows = 160;

		std::array<BenchmarkRow, maxRows> rows{};
		std::size_t rowCount{0};

		void add(std::string_view section, std::string_view metric, std::string_view value, std::string_view unit, std::string_view note = {}) {
			if (rowCount >= rows.size()) {
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
		std::atomic<std::uint32_t>* completed{nullptr};
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
		BenchmarkReport report{};
		std::array<char, lexxos::vfs::maxPathLength> reportPath{};
		std::array<char, lexxos::vfs::maxPathLength> ioPath{};
		std::array<char, ioChunkBytes> ioBuffer{};
		std::array<std::uint32_t, piSpigotWorkSize> piWork{};
		std::array<std::uint16_t, displayBenchmarkMaxWidth> displayPixels{};
		std::array<std::uint8_t, displayBenchmarkMaxWidth * rgb888BytesPerPixel> displayRgb888Row{};
		std::atomic<bool> workersStart{false};
		std::atomic<std::uint32_t> workersCompleted{0};
		std::array<BenchmarkWorker, maxWorkers> workers{};
	};

	class ReportOutputStream final : public lexxos::io::OutputStream {
	public:
		ReportOutputStream(lexxos::io::OutputStream& console, lexxos::vfs::FileHandle* file)
		    : _console(console)
		    , _file(file) {
		}

		bool write(std::string_view text) override {
			auto const consoleOk = _console.write(text);
			if (_file != nullptr && _file->isOpen() && !_failed) {
				auto const status = _file->write(text);
				if (status != lexxos::vfs::WriteStatus::Ok) {
					_failed = true;
				}
			}

			return consoleOk && !_failed;
		}

		[[nodiscard]] bool failed() const {
			return _failed;
		}

	private:
		lexxos::io::OutputStream& _console;
		lexxos::vfs::FileHandle* _file{nullptr};
		bool _failed{false};
	};

	struct BenchmarkTableContext {
		BenchmarkReport const* report{nullptr};
	};

	bool provideBenchmarkRow(std::size_t rowIndex, PrettyTableRow& row, void* userData) {
		auto const* tableContext = static_cast<BenchmarkTableContext const*>(userData);
		if (tableContext == nullptr || tableContext->report == nullptr || rowIndex >= tableContext->report->rowCount) {
			return false;
		}

		auto const& source = tableContext->report->rows[rowIndex];
		row.set(0, textView(source.section));
		row.set(1, textView(source.metric));
		row.set(2, textView(source.value));
		row.set(3, textView(source.unit));
		row.set(4, textView(source.note));
		return true;
	}

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

	void appendCoreNote(FixedText& note, int coreAffinity) {
		note.append("core=");
		if (coreAffinity < 0) {
			note.append("default");
			return;
		}
		note.appendUnsigned(static_cast<std::uint32_t>(coreAffinity));
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

	std::uint32_t parseBoundedUnsigned(lexxos::os::AppContext& context, lexxos::program::ParsedArguments const& arguments,
	    std::string_view name, std::uint32_t fallback, std::uint32_t minimum, std::uint32_t maximum) {
		std::uint32_t value = 0;
		if (!parseUnsigned32(arguments.valueOr(name, {}), value)) {
			return fallback;
		}
		if (value < minimum || value > maximum) {
			context.write("bench ");
			context.write(name);
			context.write(": expected ");
			writeUnsigned(context, minimum);
			context.write("..");
			writeUnsigned(context, maximum);
			context.writeLine(", using default");
			return fallback;
		}
		return value;
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
		worker.completed->fetch_add(1U, std::memory_order_release);
	}

	void addClockRate(BenchmarkReport& report, std::string_view metric, lexxos::platform::ClockRate rate) {
		if (!rate.available) {
			report.add("clock", metric, "n/a", "Hz");
			return;
		}
		report.addUnsigned("clock", metric, rate.hz, "Hz");
	}

	void addConfiguredBusRates(BenchmarkReport& report, lexxos::platform::BoardDescriptor const& board) {
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

	void addSdRuntimeRows(BenchmarkReport& report) {
#if FEATURE_ENABLED(SD_CARD)
		report.add("sd", "backend", lexxos::devices::toString(lexxos::devices::sd_card::interfaceType()), "", "runtime");
		report.add("sd", "state", lexxos::devices::toString(lexxos::devices::sd_card::state()), "", "runtime");
		report.addUnsigned("sd", "active-baud", lexxos::devices::sd_card::activeBaudRate(), "Hz");
		report.add("sd", "multi-read", lexxos::devices::sd_card::multiBlockReadEnabled() ? "on" : "off", "");
		report.add("sd", "multi-write", lexxos::devices::sd_card::multiBlockWriteEnabled() ? "on" : "off", "");
		report.add("sd", "dma", lexxos::devices::sd_card::dmaTransfersEnabled() ? "on" : "off", "");
#else
		report.add("sd", "support", "disabled", "");
#endif
	}

	void collectConfiguration(lexxos::os::AppContext& context, BenchmarkReport& report) {
		auto const& board = activeBoard(context);
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
		addSdRuntimeRows(report);

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

	void runMemoryBenchmark(lexxos::os::AppContext& context, BenchmarkReport& report, BenchmarkWorkspace& workspace, std::uint32_t iterations) {
		std::uint32_t const passes = std::max<std::uint32_t>(iterations / 16U, 1U);
		std::array<char, ioChunkBytes> target{};
		for (std::size_t index = 0; index < workspace.ioBuffer.size(); ++index) {
			workspace.ioBuffer[index] = static_cast<char>((index * 17U) & 0x7FU);
		}

		auto const start = uptimeMs(context);
		for (std::uint32_t pass = 0; pass < passes; ++pass) {
			std::memcpy(target.data(), workspace.ioBuffer.data(), target.size());
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
		workspace.workersCompleted.store(0, std::memory_order_release);

		std::uint32_t createdWorkers = 0;
		std::uint32_t const workerIterations = std::max<std::uint32_t>(iterations / requestedWorkers, minIterations);
		for (std::uint32_t index = 0; index < requestedWorkers; ++index) {
			auto& worker = workspace.workers[index];
			worker.start = &workspace.workersStart;
			worker.completed = &workspace.workersCompleted;
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
		while (workspace.workersCompleted.load(std::memory_order_acquire) < createdWorkers) {
			sleepMs(context, 1);
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

		lexxos::vfs::FileHandle output;
		auto openStatus = lexxos::vfs::open(textView(workspace.ioPath), lexxos::vfs::OpenMode::CreateTruncateWrite, output);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			report.add("vfs", "write-open", "failed", "", lexxos::vfs::toString(openStatus));
			return;
		}

		auto const writeStart = uptimeMs(context);
		for (std::uint32_t offset = 0; offset < ioBenchmarkBytes; offset += ioChunkBytes) {
			fillIoPattern(workspace.ioBuffer, offset / ioChunkBytes);
			auto const status = output.write(std::string_view{workspace.ioBuffer.data(), workspace.ioBuffer.size()});
			if (status != lexxos::vfs::WriteStatus::Ok) {
				output.close();
				report.add("vfs", "write", "failed", "", lexxos::vfs::toString(status));
				return;
			}
		}
		output.close();
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
		while (readBytes < ioBenchmarkBytes) {
			std::size_t bytesRead = 0;
			auto const status = input.read(workspace.ioBuffer.data(), workspace.ioBuffer.size(), bytesRead);
			if (status != lexxos::vfs::ReadStatus::Ok || bytesRead == 0) {
				input.close();
				auto const note = status == lexxos::vfs::ReadStatus::Ok ? std::string_view{"short read"} : lexxos::vfs::toString(status);
				report.add("vfs", "read", "failed", "", note);
				return;
			}
			readBytes += static_cast<std::uint32_t>(bytesRead);
		}
		input.close();
		auto const readElapsed = elapsedMs(readStart, uptimeMs(context));
		(void) lexxos::vfs::remove(textView(workspace.ioPath));

		report.addUnsigned("vfs", "write-4k", writeElapsed, "ms", textView(workspace.ioPath));
		report.addUnsigned("vfs", "write-rate", perSecond(ioBenchmarkBytes / 1024U, writeElapsed), "KiB/s");
		report.addUnsigned("vfs", "read-4k", readElapsed, "ms", textView(workspace.ioPath));
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
	    BenchmarkWorkspace& workspace,
	    lexxos::vfs::FileHandle& file) {
		auto const requestedPath = arguments.valueOr("out", defaultReportPath);
		auto const defaultPathRequested = requestedPath.empty() || !arguments.has("out");
		if (defaultPathRequested) {
			buildTimestampedReportPath(context, arguments, defaultReportDirectory, workspace.reportPath);
			(void) lexxos::vfs::createDirectory(defaultReportDirectory);
		} else {
			copyText(workspace.reportPath, requestedPath);
		}

		auto status = lexxos::vfs::open(textView(workspace.reportPath), lexxos::vfs::OpenMode::CreateTruncateWrite, file);
		if (status == lexxos::vfs::OpenStatus::Ok) {
			return true;
		}

		if (defaultPathRequested) {
			buildTimestampedReportPath(context, arguments, fallbackReportDirectory, workspace.reportPath);
			status = lexxos::vfs::open(textView(workspace.reportPath), lexxos::vfs::OpenMode::CreateTruncateWrite, file);
			if (status == lexxos::vfs::OpenStatus::Ok) {
				return true;
			}
		}

		context.write("bench: report save disabled, open failed: ");
		context.writeLine(lexxos::vfs::toString(status));
		return false;
	}

	void emitReport(lexxos::os::AppContext& context, BenchmarkReport const& report, lexxos::vfs::FileHandle* file) {
		constexpr PrettyTableColumn columns[] = {
		    {.name = "section",  .align = PrettyTableAlign::Left, .minWidth = 7, .maxWidth = 14},
		    { .name = "metric",  .align = PrettyTableAlign::Left, .minWidth = 8, .maxWidth = 22},
		    {  .name = "value", .align = PrettyTableAlign::Right, .minWidth = 6, .maxWidth = 18},
		    {   .name = "unit",  .align = PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 10},
		    {   .name = "note",  .align = PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 32},
		};

		ReportOutputStream output{context.standardOutput(), file};
		auto reportContext = context.createChild({
		    .standardOutput = &output,
		    .standardError = &output,
		});

		reportContext.writeLine("LexxOS benchmark report");
		BenchmarkTableContext tableContext{.report = &report};
		PrettyTable table{columns, sizeof(columns) / sizeof(columns[0])};
		table.setStyle({
		    .border = true,
		    .header = true,
		    .horizontalRules = PrettyTableRules::Header,
		    .leftPadding = 1,
		    .rightPadding = 1,
		    .maxTableWidth = 120,
		});
		table.print(reportContext, provideBenchmarkRow, &tableContext);
		if (output.failed()) {
			context.writeLine("bench: report file write failed");
		}
	}

	void announceStep(lexxos::os::AppContext& context, std::string_view step) {
		context.write("bench: ");
		context.writeLine(step);
	}

	BenchmarkWorkspace* allocateWorkspace(lexxos::os::AppContext& context) {
		auto* arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("bench: program memory arena is unavailable");
			return nullptr;
		}

		auto* memory = arena->allocate(sizeof(BenchmarkWorkspace), alignof(BenchmarkWorkspace));
		if (memory == nullptr) {
			context.writeLine("bench: not enough program workspace memory");
			return nullptr;
		}
		return new (memory) BenchmarkWorkspace{};
	}

} // namespace

ProgramDescriptor BenchmarkProgram::descriptor() {
	return {
	    .name = "bench",
	    .help = "run portable LexxOS system benchmark",
	    .description = "Collects board/runtime settings, runs CPU, memory, scheduler, multitask and VFS synthetic tests, then prints and saves a table report.",
	    .usage = "bench [--iterations count] [--tasks count] [--out path] [--io-path path] [--no-save]",
	    .arguments = argumentList(benchmarkArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(BenchmarkWorkspace)),
	                .cancellable = true,
	                .allowBackground = true,
	                },
	};
}

int BenchmarkProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	context.writeLine("bench: collecting configuration and running synthetic tests");

	auto* workspace = allocateWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto iterations = parseBoundedUnsigned(context, arguments, "iterations", defaultIterations, minIterations, maxIterations);
	auto workers = parseBoundedUnsigned(context, arguments, "tasks", defaultWorkers, 1U, maxWorkers);

	announceStep(context, "configuration");
	collectConfiguration(context, workspace->report);
	announceStep(context, "cpu");
	runCpuBenchmark(context, workspace->report, iterations);
	announceStep(context, "memory");
	runMemoryBenchmark(context, workspace->report, *workspace, iterations);
	announceStep(context, "yield");
	runYieldBenchmark(context, workspace->report, iterations);
	announceStep(context, "platform sleep");
	runSleepBenchmark(context, workspace->report);
	announceStep(context, "math");
	runArithmeticMicroBenchmark(context, workspace->report, *workspace, iterations);
	announceStep(context, "rtos tick");
	runRtosTickBenchmark(context, workspace->report);
	announceStep(context, "rtos primitives");
	runRtosPrimitiveBenchmark(context, workspace->report);
	announceStep(context, "multitask");
	runMultitaskBenchmarks(context, workspace->report, *workspace, iterations, workers);
	announceStep(context, "vfs");
	runVfsIoBenchmark(context, workspace->report, *workspace, arguments);
	announceStep(context, "console");
	runConsoleMirrorBenchmark(context, workspace->report);
	announceStep(context, "display");
	runDirectDisplayBenchmark(context, workspace->report, *workspace);
	runDirectTextAreaFillBenchmark(context, workspace->report, *workspace);
	announceStep(context, "render");
	runConsoleRendererStatsBenchmark(context, workspace->report);
	runConsoleClearBenchmark(context, workspace->report);

	lexxos::vfs::FileHandle reportFile;
	lexxos::vfs::FileHandle* reportFilePtr = nullptr;
	if (!arguments.has("no-save") && openReportFile(context, arguments, *workspace, reportFile)) {
		reportFilePtr = &reportFile;
		workspace->report.add("output", "report", textView(workspace->reportPath), "", "saved");
	} else if (arguments.has("no-save")) {
		workspace->report.add("output", "report", "disabled", "", "--no-save");
	}

	emitReport(context, workspace->report, reportFilePtr);
	if (reportFile.isOpen()) {
		reportFile.close();
	}

	auto const memory = lexxos::kernel::memorySnapshot();
	if (memory.available) {
		context.write("bench: free heap after benchmark: ");
		writeUnsigned(context, memory.freeHeapBytes);
		context.writeLine(" bytes");
	}
	return 0;
}

} // namespace lexxos::apps::system_programs
