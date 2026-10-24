#include <lexxos/apps/devices/display_test_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

#if FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>
#endif

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <portable.h>
#endif

namespace lexxos::apps::device_programs {
namespace {

	constexpr int maxBenchmarkWidth = detail::displayScratchLinePixels;
	constexpr int defaultBenchmarkRowsPerChunk = 16;
	constexpr int maxBenchmarkRowsPerChunk = 64;
	constexpr std::size_t benchmarkHeapGuardBytes = 4096;
	constexpr std::uint32_t defaultBenchmarkFrames = 20;
	constexpr std::uint32_t maxBenchmarkFrames = 500;
	constexpr std::uint32_t defaultPsramAddress = 0x00200000U;
	constexpr std::uint32_t rowsScanValues[] = {1, 2, 4, 8, 16, 24, 32, 48, 64};

	enum class BenchmarkTransport : std::uint8_t {
		RowBlit,
		Stream,
		DoubleBuffer,
		CachedCopy,
		Psram,
		All,
	};

	struct BenchmarkCase {
		std::string_view name;
		int width{0};
		int height{0};
	};

	struct BenchmarkRect {
		int x{0};
		int y{0};
		int width{0};
		int height{0};
	};

	struct BenchmarkOptions {
		BenchmarkTransport transport{BenchmarkTransport::Stream};
		bool matrix{false};
		bool bench{false};
		bool rowsScan{false};
		bool keepBaud{false};
		std::uint32_t baud{0};
		std::uint32_t frames{defaultBenchmarkFrames};
		std::uint32_t requestedWidth{0};
		std::uint32_t requestedHeight{0};
		std::uint32_t requestedX{0};
		std::uint32_t requestedY{0};
		bool hasX{false};
		bool hasY{false};
		std::uint32_t rowsPerChunk{defaultBenchmarkRowsPerChunk};
		std::uint32_t psramAddress{defaultPsramAddress};
		std::string_view size{"full"};
	};

	struct BenchmarkResult {
		bool ok{false};
		std::uint32_t elapsedMs{0};
		std::uint64_t logicalBytes{0};
		std::uint64_t wireBytes{0};
	};

	struct BenchmarkWorkspace {
		BenchmarkWorkspace() = default;
		BenchmarkWorkspace(BenchmarkWorkspace const&) = delete;
		BenchmarkWorkspace& operator=(BenchmarkWorkspace const&) = delete;

		~BenchmarkWorkspace() {
			release();
		}

		std::uint8_t* storage{nullptr};
		std::uint8_t* bufferA{nullptr};
		std::uint8_t* bufferB{nullptr};
		std::size_t bufferBytes{0};
		std::uint8_t bufferCount{0};

		void release();
		bool ensure(lexxos::os::AppContext& context, std::size_t bytes, std::uint8_t requestedBufferCount);
	};

	constexpr lexxos::program::ProgramArgumentSpec displayTestArguments[] = {
	    {
         .name = "name",
         .valueName = "name",
         .help = "device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "bench",
         .help = "run display throughput benchmark instead of the visual gradient",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "matrix",
         .help = "run standard benchmark matrix: full, gb2, gb240 and gb1 sizes",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "rows-scan",
         .help = "scan double-buffer rows-per-chunk values for the selected benchmark size",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "transport",
         .valueName = "mode",
         .help = "benchmark transport: row, stream, double, cached, psram or all",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "stream",
	     },
	    {
         .name = "size",
         .valueName = "name",
         .help = "benchmark size: full, gb2, gb240, gb1 or custom",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "full",
	     },
	    {
         .name = "baud",
         .valueName = "hz",
         .help = "temporarily set display SPI baud rate before benchmark",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "keep-baud",
         .help = "do not restore the previous display bus speed after benchmark",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "frames",
         .valueName = "count",
         .help = "benchmark frame count",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "20",
	     },
	    {
         .name = "width",
         .valueName = "px",
         .help = "custom benchmark width",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "height",
         .valueName = "px",
         .help = "custom benchmark height",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "x",
         .valueName = "px",
         .help = "benchmark rectangle x offset; centered when omitted",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "y",
         .valueName = "px",
         .help = "benchmark rectangle y offset; centered when omitted",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "rows",
         .valueName = "count",
         .help = "rows per chunk for double/psram transports, max 64",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "16",
	     },
	    {
         .name = "psram-address",
         .valueName = "addr",
         .help = "PSRAM staging address for psram transport",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "2097152",
	     },
	};

	void* allocateBenchmarkBytes(std::size_t bytes) {
		if (bytes == 0) {
			return nullptr;
		}
#if defined(LEXXOS_KERNEL_FREERTOS)
		if (bytes > xPortGetFreeHeapSize() || xPortGetFreeHeapSize() - bytes < benchmarkHeapGuardBytes) {
			return nullptr;
		}
		return pvPortMalloc(bytes);
#else
		return std::malloc(bytes);
#endif
	}

	void freeBenchmarkBytes(void* memory) {
		if (memory == nullptr) {
			return;
		}
#if defined(LEXXOS_KERNEL_FREERTOS)
		vPortFree(memory);
#else
		std::free(memory);
#endif
	}

	void BenchmarkWorkspace::release() {
		freeBenchmarkBytes(storage);
		storage = nullptr;
		bufferA = nullptr;
		bufferB = nullptr;
		bufferBytes = 0;
		bufferCount = 0;
	}

	bool BenchmarkWorkspace::ensure(
	    lexxos::os::AppContext& context,
	    std::size_t bytes,
	    std::uint8_t requestedBufferCount) {
		if (requestedBufferCount == 0) {
			return true;
		}
		if (bufferBytes >= bytes && bufferCount >= requestedBufferCount) {
			return true;
		}

		release();
		if (bytes > static_cast<std::size_t>(-1) / requestedBufferCount) {
			context.writeErrorLine("display-test: benchmark workspace request is too large");
			return false;
		}

		auto const totalBytes = bytes * requestedBufferCount;
		storage = static_cast<std::uint8_t*>(allocateBenchmarkBytes(totalBytes));
		if (storage == nullptr) {
			release();
			context.writeErrorLine("display-test: not enough heap for benchmark workspace");
			context.write("requested workspace bytes: ");
			writeUnsigned64(context, totalBytes);
			context.writeLine("");
#if defined(LEXXOS_KERNEL_FREERTOS)
			context.write("free heap bytes: ");
			writeUnsigned64(context, xPortGetFreeHeapSize());
			context.writeLine("");
			context.write("heap guard bytes: ");
			writeUnsigned64(context, benchmarkHeapGuardBytes);
			context.writeLine("");
#endif
			return false;
		}

		bufferA = storage;
		bufferB = requestedBufferCount > 1 ? storage + bytes : nullptr;
		bufferBytes = bytes;
		bufferCount = requestedBufferCount;
		return true;
	}

	bool parseOptionalUnsigned(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t& value) {
		auto const text = arguments.value(name);
		if (text.empty()) {
			return true;
		}
		return parseUnsigned32(text, value);
	}

	bool parseTransport(std::string_view text, BenchmarkTransport& transport) {
		if (text == "row" || text == "blit") {
			transport = BenchmarkTransport::RowBlit;
			return true;
		}

		if (text == "stream") {
			transport = BenchmarkTransport::Stream;
			return true;
		}

		if (text == "double" || text == "double-buffer") {
			transport = BenchmarkTransport::DoubleBuffer;
			return true;
		}

		if (text == "cached" || text == "copy" || text == "cached-copy") {
			transport = BenchmarkTransport::CachedCopy;
			return true;
		}

		if (text == "psram") {
			transport = BenchmarkTransport::Psram;
			return true;
		}

		if (text == "all") {
			transport = BenchmarkTransport::All;
			return true;
		}
		return false;
	}

	std::string_view transportName(BenchmarkTransport transport) {
		switch (transport) {
			case BenchmarkTransport::RowBlit:
				return "row";
			case BenchmarkTransport::Stream:
				return "stream";
			case BenchmarkTransport::DoubleBuffer:
				return "double";
			case BenchmarkTransport::CachedCopy:
				return "cached";
			case BenchmarkTransport::Psram:
				return "psram";
			case BenchmarkTransport::All:
				return "all";
		}
		return "unknown";
	}

	std::uint16_t patternPixel(int x, int y, int width, int height, std::uint32_t frame) {
		auto const checker = ((x / 20) + (y / 20) + static_cast<int>(frame)) & 1;
		auto const red = static_cast<std::uint8_t>((x * 255) / (width > 1 ? width - 1 : 1));
		auto const green = static_cast<std::uint8_t>((y * 255) / (height > 1 ? height - 1 : 1));
		auto const blue = static_cast<std::uint8_t>(checker != 0 ? 220 : 40);
		return detail::rgb565(red, green, blue);
	}

	void fillRgb565Line(std::uint16_t* line, int width, int y, int height, std::uint32_t frame) {
		for (int x = 0; x < width; ++x) {
			line[x] = patternPixel(x, y, width, height, frame);
		}
	}

	void writeRgb565Bytes(std::uint8_t*& output, std::uint16_t pixel) {
		*output++ = static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
		*output++ = static_cast<std::uint8_t>(pixel & 0xFFU);
	}

	void fillRgb565Bytes(std::uint8_t* output, int width, int firstY, int rows, int height, std::uint32_t frame) {
		auto* out = output;
		for (int row = 0; row < rows; ++row) {
			auto const y = firstY + row;
			for (int x = 0; x < width; ++x) {
				writeRgb565Bytes(out, patternPixel(x, y, width, height, frame));
			}
		}
	}

	std::uint16_t cachedCopyPixel(int x, int width) {
		auto const safeWidth = width > 1 ? width - 1 : 1;
		auto const red = static_cast<std::uint8_t>((x * 255) / safeWidth);
		auto const green = static_cast<std::uint8_t>((x * 128) / safeWidth);
		auto const blue = static_cast<std::uint8_t>(((x / 20) & 1) != 0 ? 220 : 40);
		return detail::rgb565(red, green, blue);
	}

	void fillCachedRgb565Bytes(std::uint8_t* output, int width, int rows) {
		// Prepare one reusable chunk outside the timed copy loop.
		//
		// The cached transport intentionally measures display-copy throughput only:
		// the same bytes are sent for every chunk and every frame. Keep the cached
		// pattern independent from the absolute Y coordinate, otherwise the display
		// visibly repeats the first cached group of rows as horizontal bands.
		std::array<std::uint16_t, maxBenchmarkWidth> line{};
		for (int x = 0; x < width; ++x) {
			line[static_cast<std::size_t>(x)] = cachedCopyPixel(x, width);
		}
		auto* out = output;
		for (int row = 0; row < rows; ++row) {
			for (int x = 0; x < width; ++x) {
				writeRgb565Bytes(out, line[static_cast<std::size_t>(x)]);
			}
		}
	}

	void writeFixed2(lexxos::os::AppContext& context, std::uint32_t valueTimes100) {
		writeUnsigned(context, valueTimes100 / 100U);
		context.write(".");
		auto const fraction = valueTimes100 % 100U;
		if (fraction < 10U) {
			context.write("0");
		}
		writeUnsigned(context, fraction);
	}

	bool parseBenchmarkOptions(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    BenchmarkOptions& options) {
		options.bench = arguments.has("bench") || arguments.has("matrix") || arguments.has("rows-scan");
		options.matrix = arguments.has("matrix");
		options.rowsScan = arguments.has("rows-scan");
		options.keepBaud = arguments.has("keep-baud");
		options.hasX = arguments.has("x");
		options.hasY = arguments.has("y");
		options.size = arguments.valueOr("size", "full");

		if (!parseTransport(arguments.valueOr("transport", "stream"), options.transport)) {
			context.writeErrorLine("display-test: invalid --transport, expected row, stream, double, cached, psram or all");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "baud", options.baud)) {
			context.writeErrorLine("display-test: invalid --baud");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "frames", options.frames) || options.frames == 0 || options.frames > maxBenchmarkFrames) {
			context.writeErrorLine("display-test: invalid --frames");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "width", options.requestedWidth)
		    || options.requestedWidth > static_cast<std::uint32_t>(maxBenchmarkWidth)) {
			context.writeErrorLine("display-test: invalid --width");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "height", options.requestedHeight)) {
			context.writeErrorLine("display-test: invalid --height");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "x", options.requestedX)
		    || !parseOptionalUnsigned(arguments, "y", options.requestedY)) {
			context.writeErrorLine("display-test: invalid --x/--y");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "rows", options.rowsPerChunk)
		    || options.rowsPerChunk == 0
		    || options.rowsPerChunk > maxBenchmarkRowsPerChunk) {
			context.writeErrorLine("display-test: invalid --rows");
			return false;
		}
		if (!parseOptionalUnsigned(arguments, "psram-address", options.psramAddress)) {
			context.writeErrorLine("display-test: invalid --psram-address");
			return false;
		}
		return true;
	}

	bool rectForCase(
	    lexxos::devices::DisplayDevice const& display,
	    BenchmarkOptions const& options,
	    BenchmarkCase const& benchmarkCase,
	    BenchmarkRect& output) {
		if (benchmarkCase.width <= 0 || benchmarkCase.height <= 0 || benchmarkCase.width > display.width() || benchmarkCase.height > display.height() || benchmarkCase.width > maxBenchmarkWidth) {
			return false;
		}

		output.width = benchmarkCase.width;
		output.height = benchmarkCase.height;
		output.x = options.hasX ? static_cast<int>(options.requestedX) : (display.width() - output.width) / 2;
		output.y = options.hasY ? static_cast<int>(options.requestedY) : (display.height() - output.height) / 2;
		return output.x >= 0
		    && output.y >= 0
		    && output.x + output.width <= display.width()
		    && output.y + output.height <= display.height();
	}

	BenchmarkCase selectedCase(lexxos::devices::DisplayDevice const& display, BenchmarkOptions const& options) {
		if (options.requestedWidth > 0 || options.requestedHeight > 0 || options.size == "custom") {
			return {
			    "custom",
			    options.requestedWidth > 0 ? static_cast<int>(options.requestedWidth) : display.width(),
			    options.requestedHeight > 0 ? static_cast<int>(options.requestedHeight) : display.height(),
			};
		}
		if (options.size == "gb2") {
			return {"gb2", 320, 288};
		}
		if (options.size == "gb240") {
			return {"gb240", 320, 240};
		}
		if (options.size == "gb1") {
			return {"gb1", 160, 144};
		}
		if (options.size == "half") {
			return {"half", display.width() / 2, display.height() / 2};
		}
		return {"full", display.width(), display.height()};
	}

	void standardCases(lexxos::devices::DisplayDevice const& display, std::array<BenchmarkCase, 5>& cases, std::size_t& count) {
		count = 0;
		cases[count++] = {"full", display.width(), display.height()};
		cases[count++] = {"gb2", 320, 288};
		cases[count++] = {"gb240", 320, 240};
		cases[count++] = {"gb1", 160, 144};
		cases[count++] = {"half", display.width() / 2, display.height() / 2};
	}

	bool runRowBenchmarkFrame(lexxos::devices::DisplayDevice& display, BenchmarkRect const& rect, std::uint32_t frame) {
		auto* line = detail::displayScratchLine();
		if (!display.beginFrame()) {
			return false;
		}
		for (int y = 0; y < rect.height; ++y) {
			fillRgb565Line(line, rect.width, y, rect.height, frame);
			if (!display.blitRgb565({.x = rect.x, .y = rect.y + y, .width = rect.width, .height = 1}, line, rect.width)) {
				display.endFrame();
				return false;
			}
		}
		return display.endFrame();
	}

	bool runStreamBenchmarkFrame(lexxos::devices::DisplayDevice& display, BenchmarkRect const& rect, std::uint32_t frame) {
		auto* line = detail::displayScratchLine();
		if (!display.beginFrame()) {
			return false;
		}
		if (!display.beginRgb565Stream({.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height})) {
			display.endFrame();
			return false;
		}
		for (int y = 0; y < rect.height; ++y) {
			fillRgb565Line(line, rect.width, y, rect.height, frame);
			if (!display.writeRgb565Stream(line, rect.width)) {
				display.endRgb565Stream();
				display.endFrame();
				return false;
			}
		}
		return display.endRgb565Stream() && display.endFrame();
	}

	bool runDoubleBufferBenchmarkFrame(
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkRect const& rect,
	    std::uint32_t frame,
	    int rowsPerChunk,
	    BenchmarkWorkspace& workspace) {
		auto* const bufferA = workspace.bufferA;
		auto* const bufferB = workspace.bufferB;

		if (!display.beginFrame()) {
			return false;
		}
		if (!display.beginRgb565Stream({.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height})) {
			display.endFrame();
			return false;
		}

		bool useA = true;
		for (int y = 0; y < rect.height;) {
			auto rows = rect.height - y;
			if (rows > rowsPerChunk) {
				rows = rowsPerChunk;
			}

			auto* const buffer = useA ? bufferA : bufferB;
			fillRgb565Bytes(buffer, rect.width, y, rows, rect.height, frame);
			if (!display.writeRgb565BytesStream(buffer, rect.width * rows)) {
				display.endRgb565Stream();
				display.endFrame();
				return false;
			}
			useA = !useA;
			y += rows;
		}
		return display.endRgb565Stream() && display.endFrame();
	}

	void prepareCachedCopyBenchmark(BenchmarkRect const& rect, int rowsPerChunk, BenchmarkWorkspace& workspace) {
		auto rows = rect.height;
		if (rows > rowsPerChunk) {
			rows = rowsPerChunk;
		}
		fillCachedRgb565Bytes(workspace.bufferA, rect.width, rows);
	}

	bool runCachedCopyBenchmarkFrame(
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkRect const& rect,
	    int rowsPerChunk,
	    BenchmarkWorkspace& workspace) {
		auto* const buffer = workspace.bufferA;

		if (!display.beginFrame()) {
			return false;
		}
		if (!display.beginRgb565Stream({.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height})) {
			display.endFrame();
			return false;
		}

		for (int y = 0; y < rect.height;) {
			auto rows = rect.height - y;
			if (rows > rowsPerChunk) {
				rows = rowsPerChunk;
			}
			if (!display.writeRgb565BytesStream(buffer, rect.width * rows)) {
				display.endRgb565Stream();
				display.endFrame();
				return false;
			}
			y += rows;
		}
		return display.endRgb565Stream() && display.endFrame();
	}

	bool runPsramBenchmarkFrame(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkRect const& rect,
	    std::uint32_t frame,
	    int rowsPerChunk,
	    std::uint32_t psramAddress,
	    BenchmarkWorkspace& workspace) {
#if FEATURE_ENABLED(PICO_CALC)
		auto const& board = activeBoard(context);
		if (!board.psram.spi.isValid()) {
			context.writeErrorLine("display-test: PSRAM is not configured for this board");
			return false;
		}

		auto* const writeBuffer = workspace.bufferA;
		auto* const readBuffer = workspace.bufferB;

		if (!display.beginFrame()) {
			return false;
		}
		if (!display.beginRgb565Stream({.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height})) {
			display.endFrame();
			return false;
		}

		for (int y = 0; y < rect.height;) {
			auto rows = rect.height - y;
			if (rows > rowsPerChunk) {
				rows = rowsPerChunk;
			}
			auto const bytes = static_cast<std::size_t>(rect.width * rows * 2);
			auto const address = psramAddress + static_cast<std::uint32_t>(y * rect.width * 2);

			fillRgb565Bytes(writeBuffer, rect.width, y, rows, rect.height, frame);
			if (!lexxos::apps::picocalc_programs::detail::writePsramMemory(board.psram, address, writeBuffer, bytes)
			    || !lexxos::apps::picocalc_programs::detail::readPsramMemory(board.psram, address, readBuffer, bytes)) {
				display.endRgb565Stream();
				display.endFrame();
				context.writeErrorLine("display-test: PSRAM frame staging failed");
				return false;
			}
			if (!display.writeRgb565BytesStream(readBuffer, rect.width * rows)) {
				display.endRgb565Stream();
				display.endFrame();
				return false;
			}
			y += rows;
		}
		return display.endRgb565Stream() && display.endFrame();
#else
		(void) context;
		(void) display;
		(void) rect;
		(void) frame;
		(void) rowsPerChunk;
		(void) psramAddress;
		context.writeErrorLine("display-test: PSRAM transport is only available for PicoCalc builds");
		return false;
#endif
	}

	std::uint8_t benchmarkBufferCount(BenchmarkTransport transport) {
		switch (transport) {
			case BenchmarkTransport::DoubleBuffer:
			case BenchmarkTransport::Psram:
				return 2;
			case BenchmarkTransport::CachedCopy:
				return 1;
			case BenchmarkTransport::RowBlit:
			case BenchmarkTransport::Stream:
			case BenchmarkTransport::All:
				return 0;
		}
		return 0;
	}

	bool ensureBenchmarkWorkspace(
	    lexxos::os::AppContext& context,
	    BenchmarkWorkspace& workspace,
	    BenchmarkTransport transport,
	    BenchmarkRect const& rect,
	    BenchmarkOptions const& options) {
		auto const bufferCount = benchmarkBufferCount(transport);
		if (bufferCount == 0) {
			return true;
		}

		auto const rows = rect.height < static_cast<int>(options.rowsPerChunk)
		    ? rect.height
		    : static_cast<int>(options.rowsPerChunk);
		auto const bytes = static_cast<std::size_t>(rect.width) * static_cast<std::size_t>(rows) * 2U;
		return workspace.ensure(context, bytes, bufferCount);
	}

	BenchmarkResult runBenchmark(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkTransport transport,
	    BenchmarkRect const& rect,
	    BenchmarkOptions const& options,
	    BenchmarkWorkspace& workspace) {
		if (!ensureBenchmarkWorkspace(context, workspace, transport, rect, options)) {
			return {
			    .ok = false,
			    .elapsedMs = 0,
			    .logicalBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * 2ULL * options.frames,
			    .wireBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * static_cast<std::uint64_t>(display.wireBytesPerPixel()) * options.frames,
			};
		}

		if (transport == BenchmarkTransport::CachedCopy) {
			prepareCachedCopyBenchmark(rect, static_cast<int>(options.rowsPerChunk), workspace);
		}

		auto const startMs = uptimeMs(context);
		bool ok = true;
		for (std::uint32_t frame = 0; frame < options.frames; ++frame) {
			if (context.pollCancellationRequest()) {
				ok = false;
				break;
			}

			switch (transport) {
				case BenchmarkTransport::RowBlit:
					ok = runRowBenchmarkFrame(display, rect, frame);
					break;
				case BenchmarkTransport::Stream:
					ok = runStreamBenchmarkFrame(display, rect, frame);
					break;
				case BenchmarkTransport::DoubleBuffer:
					ok = runDoubleBufferBenchmarkFrame(display, rect, frame, static_cast<int>(options.rowsPerChunk), workspace);
					break;
				case BenchmarkTransport::CachedCopy:
					ok = runCachedCopyBenchmarkFrame(display, rect, static_cast<int>(options.rowsPerChunk), workspace);
					break;
				case BenchmarkTransport::Psram:
					ok = runPsramBenchmarkFrame(context, display, rect, frame, static_cast<int>(options.rowsPerChunk), options.psramAddress, workspace);
					break;
				case BenchmarkTransport::All:
					ok = false;
					break;
			}
			if (!ok) {
				break;
			}
			lexxos::kernel::yield();
		}

		auto const elapsed = uptimeMs(context) - startMs;
		return {
		    .ok = ok,
		    .elapsedMs = elapsed,
		    .logicalBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * 2ULL * options.frames,
		    .wireBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * static_cast<std::uint64_t>(display.wireBytesPerPixel()) * options.frames,
		};
	}

	void writeBenchmarkResult(
	    lexxos::os::AppContext& context,
	    BenchmarkCase const& benchmarkCase,
	    BenchmarkTransport transport,
	    BenchmarkRect const& rect,
	    BenchmarkOptions const& options,
	    BenchmarkResult const& result) {
		context.write("display-test: case=");
		context.write(benchmarkCase.name);
		context.write(" transport=");
		context.write(transportName(transport));
		context.write(" rect=");
		writeUnsigned(context, static_cast<std::uint32_t>(rect.width));
		context.write("x");
		writeUnsigned(context, static_cast<std::uint32_t>(rect.height));
		context.write(" @ ");
		writeUnsigned(context, static_cast<std::uint32_t>(rect.x));
		context.write(",");
		writeUnsigned(context, static_cast<std::uint32_t>(rect.y));
		context.write(" frames=");
		writeUnsigned(context, options.frames);
		if (transport == BenchmarkTransport::DoubleBuffer
		    || transport == BenchmarkTransport::CachedCopy
		    || transport == BenchmarkTransport::Psram) {
			context.write(" rows=");
			writeUnsigned(context, options.rowsPerChunk);
		}
		context.write(" ms=");
		writeUnsigned(context, result.elapsedMs);
		context.write(" logical-rgb565-bytes=");
		writeUnsigned64(context, result.logicalBytes);
		context.write(" wire-bytes=");
		writeUnsigned64(context, result.wireBytes);

		if (result.elapsedMs > 0) {
			auto const fpsTimes100 =
			    static_cast<std::uint32_t>((static_cast<std::uint64_t>(options.frames) * 100000ULL) / result.elapsedMs);
			auto const kibPerSecond =
			    static_cast<std::uint32_t>((result.logicalBytes * 1000ULL) / (static_cast<std::uint64_t>(result.elapsedMs) * 1024ULL));
			auto const wireKibPerSecond =
			    static_cast<std::uint32_t>((result.wireBytes * 1000ULL) / (static_cast<std::uint64_t>(result.elapsedMs) * 1024ULL));
			context.write(" fps=");
			writeFixed2(context, fpsTimes100);
			context.write(" logical-kib-s=");
			writeUnsigned(context, kibPerSecond);
			context.write(" wire-kib-s=");
			writeUnsigned(context, wireKibPerSecond);
		}

		context.write(" ok=");
		context.writeLine(result.ok ? "yes" : "no");
	}

	bool runBenchmarkCase(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkCase const& benchmarkCase,
	    BenchmarkTransport transport,
	    BenchmarkOptions const& options,
	    BenchmarkWorkspace& workspace) {
		BenchmarkRect rect{};
		if (!rectForCase(display, options, benchmarkCase, rect)) {
			context.write("display-test: skip case=");
			context.write(benchmarkCase.name);
			context.writeLine(" unsupported by current display");
			return true;
		}

		if (transport == BenchmarkTransport::All) {
			constexpr BenchmarkTransport transports[] = {
			    BenchmarkTransport::RowBlit,
			    BenchmarkTransport::Stream,
			    BenchmarkTransport::DoubleBuffer,
			    BenchmarkTransport::CachedCopy,
			    BenchmarkTransport::Psram,
			};
			for (auto const nestedTransport : transports) {
				auto const result = runBenchmark(context, display, nestedTransport, rect, options, workspace);
				writeBenchmarkResult(context, benchmarkCase, nestedTransport, rect, options, result);
				if (!result.ok) {
					return false;
				}
			}
			return true;
		}

		auto const result = runBenchmark(context, display, transport, rect, options, workspace);
		writeBenchmarkResult(context, benchmarkCase, transport, rect, options, result);
		return result.ok;
	}

	bool runRowsScanForCase(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkCase const& benchmarkCase,
	    BenchmarkOptions const& options,
	    BenchmarkWorkspace& workspace) {
		BenchmarkRect rect{};
		if (!rectForCase(display, options, benchmarkCase, rect)) {
			context.write("display-test: skip rows-scan case=");
			context.write(benchmarkCase.name);
			context.writeLine(" unsupported by current display");
			return true;
		}

		for (auto const rows : rowsScanValues) {
			if (rows > static_cast<std::uint32_t>(rect.height)) {
				continue;
			}

			auto scanOptions = options;
			scanOptions.transport = BenchmarkTransport::DoubleBuffer;
			scanOptions.rowsPerChunk = rows;
			auto const result = runBenchmark(context, display, BenchmarkTransport::DoubleBuffer, rect, scanOptions, workspace);
			writeBenchmarkResult(context, benchmarkCase, BenchmarkTransport::DoubleBuffer, rect, scanOptions, result);
			if (!result.ok) {
				return false;
			}
		}
		return true;
	}

	bool runRowsScan(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkOptions const& options,
	    BenchmarkWorkspace& workspace) {
		if (!options.matrix) {
			return runRowsScanForCase(context, display, selectedCase(display, options), options, workspace);
		}

		std::array<BenchmarkCase, 5> cases{};
		std::size_t caseCount = 0;
		standardCases(display, cases, caseCount);
		for (std::size_t index = 0; index < caseCount; ++index) {
			if (!runRowsScanForCase(context, display, cases[index], options, workspace)) {
				return false;
			}
		}
		return true;
	}

	bool restoreDisplayBaud(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    std::uint32_t originalBaud,
	    bool keepBaud) {
		if (keepBaud || originalBaud == 0) {
			return true;
		}
		if (!display.configureBusSpeed(originalBaud)) {
			context.writeErrorLine("display-test: warning: failed to restore display bus speed");
			return false;
		}
		return true;
	}

	int runBenchmarkMode(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkOptions const& options) {
		BenchmarkWorkspace workspace{};

		auto const originalBaud = display.busSpeedHz();
		if (options.baud > 0) {
			if (!display.configureBusSpeed(options.baud)) {
				context.writeErrorLine("display-test: display bus speed change is unsupported or busy");
				return 1;
			}
		}

		context.write("display-test: benchmark display=");
		context.write(display.name());
		context.write(" size=");
		writeUnsigned(context, static_cast<std::uint32_t>(display.width()));
		context.write("x");
		writeUnsigned(context, static_cast<std::uint32_t>(display.height()));
		context.write(" native=");
		context.write(lexxos::devices::toString(display.nativeFormat()));
		context.write(" wire=");
		context.write(lexxos::devices::toString(display.wireFormat()));
		context.write(" wire-bpp=");
		writeUnsigned(context, static_cast<std::uint32_t>(display.wireBytesPerPixel()));
		context.write(" bus-hz=");
		writeUnsigned(context, display.busSpeedHz());
		context.writeLine("");

		bool ok = true;
		{
			auto graphicsMode = detail::enterGraphicsMode();
			(void) graphicsMode;

			if (options.rowsScan) {
				ok = runRowsScan(context, display, options, workspace);
			} else if (options.matrix) {
				std::array<BenchmarkCase, 5> cases{};
				std::size_t caseCount = 0;
				standardCases(display, cases, caseCount);
				for (std::size_t index = 0; index < caseCount; ++index) {
					if (!runBenchmarkCase(context, display, cases[index], options.transport, options, workspace)) {
						ok = false;
						break;
					}
				}
			} else {
				ok = runBenchmarkCase(context, display, selectedCase(display, options), options.transport, options, workspace);
			}
		}

		(void) restoreDisplayBaud(context, display, originalBaud, options.keepBaud);
		return ok ? 0 : 1;
	}

	int runPatternMode(lexxos::os::AppContext& context, lexxos::devices::DisplayDevice& display) {
		auto* line = detail::displayScratchLine();
		auto const width = display.width();
		auto const height = display.height();
		if (width <= 0 || height <= 0 || width > detail::displayScratchLinePixels) {
			context.writeErrorLine("display-test: unsupported display size");
			return 1;
		}

		char const* error = nullptr;
		{
			auto graphicsMode = detail::enterGraphicsMode();
			(void) graphicsMode;

			if (!display.beginFrame()) {
				error = "display-test: failed to begin frame";
			} else {
				for (int y = 0; y < height; ++y) {
					fillRgb565Line(line, width, y, height, 0);
					if (!display.blitRgb565({.x = 0, .y = y, .width = width, .height = 1}, line, width)) {
						error = "display-test: failed to draw frame";
						break;
					}
				}
				if (error == nullptr && !display.endFrame()) {
					error = "display-test: failed to end frame";
				}
			}
		}

		if (error != nullptr) {
			context.writeErrorLine(error);
			return 1;
		}

		context.write("drawn: ");
		writeUnsigned(context, static_cast<std::uint32_t>(width));
		context.write("x");
		writeUnsigned(context, static_cast<std::uint32_t>(height));
		context.write(" native=");
		context.write(lexxos::devices::toString(display.nativeFormat()));
		context.write(" wire=");
		context.write(lexxos::devices::toString(display.wireFormat()));
		context.write(" wire-bpp=");
		writeUnsigned(context, static_cast<std::uint32_t>(display.wireBytesPerPixel()));
		context.write(" bus-hz=");
		writeUnsigned(context, display.busSpeedHz());
		context.writeLine("");
		return 0;
	}

} // namespace

lexxos::program::ProgramDescriptor DisplayTestProgram::descriptor() {
	return {
	    .name = "display-test",
	    .help = "draw a test pattern or benchmark display throughput",
	    .description = "Draws a RGB565 test pattern or measures display throughput across row, stream, double-buffer, cached-copy and PSRAM-staged paths.",
	    .usage = "display-test [name] [--bench] [--matrix] [--rows-scan] [--transport row|stream|double|cached|psram|all] [--size full|gb2|gb240|gb1|custom] [--baud <hz>] [--frames <count>] [--rows <count>] [--width <px>] [--height <px>]",
	    .arguments = argumentList(displayTestArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                .interfaceKind = lexxos::program::ProgramInterfaceKind::Graphical,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = detail::selectedDisplay(context, arguments.positional("name"));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "display-test: display device is not registered"
		        : "display-test: display device is not ready");
		return 1;
	}

	BenchmarkOptions options{};
	if (!parseBenchmarkOptions(context, arguments, options)) {
		return 2;
	}

	auto diagnosticLocks = detail::lockDisplayDiagnosticResources(context, *display, "display-test");
	if (!diagnosticLocks) {
		context.writeErrorLine("display-test: display or input device is busy");
		return 1;
	}

	if (options.bench) {
		return runBenchmarkMode(context, *display, options);
	}

	return runPatternMode(context, *display);
}

} // namespace lexxos::apps::device_programs
