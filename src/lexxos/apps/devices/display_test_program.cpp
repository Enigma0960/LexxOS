#include <lexxos/apps/devices/display_test_program.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_RK3506)
	#include <lexxos/composition/products/lyra_spi_queues.h>
	#include <lexxos/composition/products/picocalc_lyra_device_providers.h>
	#include <lexxos/devices/bus/bus_transaction.h>
#endif

#if defined(LEXXOS_PLATFORM_RK3506)
	#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
	#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>
#endif

#if FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>
#endif

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <portable.h>
#endif

#if defined(LEXXOS_PLATFORM_RK3506)
extern "C" void* rk3506_heap_allocate(std::size_t bytes);
extern "C" void rk3506_heap_release(void* pointer);
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
		CachedNative,
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
#if defined(LEXXOS_PLATFORM_RK3506)
		lexxos::platform::rk3506::LcdSpi0Statistics spi{};
		lexxos::devices::DoubleBufferedDisplayTiming frameTiming{};
		lexxos::devices::Ili9488DisplayTiming panelTiming{};
#endif
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
#if defined(LEXXOS_PLATFORM_RK3506)
	    {
	     .name = "lcd-dma",
	     .help = "report and enable PL330 SPI0 TX DMA during benchmark",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "lcd-direct16",
	     .help = "force native RGB565 halfword DMA for this benchmark",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "lcd-packed8",
	     .help = "temporarily use 8-bit packed RGB565 instead of board default direct16",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
#endif
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
	     .help = "benchmark transport: row, stream, double, cached, native, psram or all",
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

#elif defined(LEXXOS_PLATFORM_RK3506)
		return rk3506_heap_allocate(bytes);
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
#elif defined(LEXXOS_PLATFORM_RK3506)
		rk3506_heap_release(memory);
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

		if (text == "native" || text == "native565") {
			transport = BenchmarkTransport::CachedNative;
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
			case BenchmarkTransport::CachedNative:
				return "native";
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

	void prepareCachedNativeBenchmark(BenchmarkRect const& rect, int rowsPerChunk, BenchmarkWorkspace& workspace) {
		auto const rows = std::min(rect.height, rowsPerChunk);
		auto* output = reinterpret_cast<std::uint16_t*>(workspace.bufferA);
		for (int y = 0; y < rows; ++y) {
			for (int x = 0; x < rect.width; ++x) {
				output[static_cast<std::size_t>(y) * rect.width + x] = cachedCopyPixel(x, rect.width);
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
			context.writeErrorLine("display-test: invalid --transport, expected row, stream, double, cached, native, psram or all");
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

	bool runCachedNativeBenchmarkFrame(
	    lexxos::devices::DisplayDevice& display,
	    BenchmarkRect const& rect,
	    int rowsPerChunk,
	    BenchmarkWorkspace& workspace) {
		auto const* source = reinterpret_cast<std::uint16_t const*>(workspace.bufferA);
		if (!display.beginFrame()) {
			return false;
		}

		if (!display.beginRgb565Stream({.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height})) {
			display.endFrame();
			return false;
		}
		for (int y = 0; y < rect.height;) {
			auto const rows = std::min(rect.height - y, rowsPerChunk);
			if (!display.writeRgb565Stream(source, rect.width * rows)) {
				display.endRgb565Stream();
				display.endFrame();
				return false;
			}

			y += rows;
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
			case BenchmarkTransport::CachedNative:
				return 1;
			case BenchmarkTransport::RowBlit:
			case BenchmarkTransport::Stream :
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
		if (!display.waitIdle() || !ensureBenchmarkWorkspace(context, workspace, transport, rect, options)) {
			return {
			    .ok = false,
			    .elapsedMs = 0,
			    .logicalBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * 2ULL * options.frames,
			    .wireBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * static_cast<std::uint64_t>(display.wireBytesPerPixel()) * options.frames,
			};
		}

		if (transport == BenchmarkTransport::CachedCopy) {
			prepareCachedCopyBenchmark(rect, static_cast<int>(options.rowsPerChunk), workspace);
		} else if (transport == BenchmarkTransport::CachedNative) {
			prepareCachedNativeBenchmark(rect, static_cast<int>(options.rowsPerChunk), workspace);
		}

#if defined(LEXXOS_PLATFORM_RK3506)
		auto const frameStart = lexxos::composition::products::picoCalcLyraDisplayTiming();
		auto const panelStart = lexxos::composition::products::picoCalcLyraPanelTiming();
		auto const spiStart = lexxos::platform::rk3506::lcdSpi0Statistics();
#endif
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
				case BenchmarkTransport::CachedNative:
					ok = runCachedNativeBenchmarkFrame(display, rect, static_cast<int>(options.rowsPerChunk), workspace);
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

		ok = display.waitIdle() && ok;
		auto const elapsed = uptimeMs(context) - startMs;
#if defined(LEXXOS_PLATFORM_RK3506)
		auto spi = lexxos::platform::rk3506::lcdSpi0Statistics();
		spi.calls -= spiStart.calls;
		spi.requestedBytes -= spiStart.requestedBytes;
		spi.completedBytes -= spiStart.completedBytes;
		spi.elapsedTicks -= spiStart.elapsedTicks;
		spi.failures -= spiStart.failures;

		auto timing = lexxos::composition::products::picoCalcLyraDisplayTiming();
		timing.copyTicks -= frameStart.copyTicks;
		timing.drawTicks -= frameStart.drawTicks;
		timing.queueTicks -= frameStart.queueTicks;
		timing.presentTicks -= frameStart.presentTicks;
		timing.copiedFrames -= frameStart.copiedFrames;
		timing.copiedBytes -= frameStart.copiedBytes;
		timing.fullSyncs -= frameStart.fullSyncs;
		timing.partialSyncs -= frameStart.partialSyncs;
		timing.skippedSyncs -= frameStart.skippedSyncs;
		timing.preparedFrames -= frameStart.preparedFrames;
		timing.presentedFrames -= frameStart.presentedFrames;
		timing.presentedRegions -= frameStart.presentedRegions;
		timing.presentedPixels -= frameStart.presentedPixels;

		auto panelTiming = lexxos::composition::products::picoCalcLyraPanelTiming();
		panelTiming.roiStageTicks -= panelStart.roiStageTicks;
		panelTiming.roiStageBytes -= panelStart.roiStageBytes;
		panelTiming.roiStageCalls -= panelStart.roiStageCalls;
#endif
		return {
		    .ok = ok,
		    .elapsedMs = elapsed,
		    .logicalBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * 2ULL * options.frames,
		    .wireBytes = static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height) * static_cast<std::uint64_t>(display.wireBytesPerPixel()) * options.frames,
#if defined(LEXXOS_PLATFORM_RK3506)
		    .spi = spi,
		    .frameTiming = timing,
		    .panelTiming = panelTiming,
#endif
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
		    || transport == BenchmarkTransport::CachedNative
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

		if (result.ok && result.elapsedMs > 0) {
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

#if defined(LEXXOS_PLATFORM_RK3506)
		if (result.spi.counterHz != 0) {
			auto const ticks = result.spi.elapsedTicks;
			auto const hz = result.spi.counterHz;
			auto const spiUs = (ticks / hz) * 1000000ULL + ((ticks % hz) * 1000000ULL) / hz;
			context.write(" spi0-us=");
			writeUnsigned64(context, spiUs);
			context.write(" spi0-calls=");
			writeUnsigned64(context, result.spi.calls);
			context.write(" spi0-completed-bytes=");
			writeUnsigned64(context, result.spi.completedBytes);
			context.write(" spi0-failures=");
			writeUnsigned64(context, result.spi.failures);

			context.write(" frame-copy-us=");
			writeUnsigned64(context, (result.frameTiming.copyTicks / hz) * 1000000ULL + ((result.frameTiming.copyTicks % hz) * 1000000ULL) / hz);
			context.write(" frame-draw-us=");
			writeUnsigned64(context, (result.frameTiming.drawTicks / hz) * 1000000ULL + ((result.frameTiming.drawTicks % hz) * 1000000ULL) / hz);
			context.write(" present-queue-us=");
			writeUnsigned64(context, (result.frameTiming.queueTicks / hz) * 1000000ULL + ((result.frameTiming.queueTicks % hz) * 1000000ULL) / hz);
			context.write(" present-write-us=");
			writeUnsigned64(context, (result.frameTiming.presentTicks / hz) * 1000000ULL + ((result.frameTiming.presentTicks % hz) * 1000000ULL) / hz);
			context.write(" copies=");
			writeUnsigned(context, result.frameTiming.copiedFrames);
			context.write(" copy-bytes=");
			writeUnsigned64(context, result.frameTiming.copiedBytes);
			context.write(" full-syncs=");
			writeUnsigned(context, result.frameTiming.fullSyncs);
			context.write(" partial-syncs=");
			writeUnsigned(context, result.frameTiming.partialSyncs);
			context.write(" skipped-syncs=");
			writeUnsigned(context, result.frameTiming.skippedSyncs);
			context.write(" roi-stage-us=");
			writeUnsigned64(context, (result.panelTiming.roiStageTicks / hz) * 1000000ULL + ((result.panelTiming.roiStageTicks % hz) * 1000000ULL) / hz);
			context.write(" roi-stage-bytes=");
			writeUnsigned64(context, result.panelTiming.roiStageBytes);
			context.write(" roi-stage-calls=");
			writeUnsigned(context, result.panelTiming.roiStageCalls);
			context.write(" prepared=");
			writeUnsigned(context, result.frameTiming.preparedFrames);
			context.write(" presented=");
			writeUnsigned(context, result.frameTiming.presentedFrames);
			context.write(" present-rects=");
			writeUnsigned64(context, result.frameTiming.presentedRegions);
			context.write(" present-pixels=");
			writeUnsigned64(context, result.frameTiming.presentedPixels);
		}
#endif

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
			    BenchmarkTransport::CachedNative,
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

#if defined(LEXXOS_PLATFORM_RK3506)
	void writeLcdDmaFailure(lexxos::os::AppContext& context) {
		auto const fault = lexxos::platform::rk3506::lcdDmaLastFailure();
		if (!fault.valid) {
			return;
		}

		context.write("display-test: lcd-dma fault-stage=");
		context.write(fault.stage);
		context.write(" manager-state=");
		writeUnsigned(context, fault.managerState);
		context.write(" manager-pc=");
		writeUnsigned(context, fault.managerPc);
		context.write(" manager-fault=");
		writeUnsigned(context, fault.managerFault);
		context.write(" manager-fault-type=");
		writeUnsigned(context, fault.managerFaultType);
		context.writeLine("");
		context.write("display-test: lcd-dma ch2-state=");
		writeUnsigned(context, fault.channelState);
		context.write(" ch2-pc=");
		writeUnsigned(context, fault.channelPc);
		context.write(" channel-faults=");
		writeUnsigned(context, fault.channelFaults);
		context.write(" ch2-fault-type=");
		writeUnsigned(context, fault.channelFaultType);
		context.write(" code-at=");
		writeUnsigned(context, fault.codeAddress);
		context.write(" code-bytes=");
		writeUnsigned(context, fault.codeBytes);
		context.writeLine("");
		context.write("display-test: lcd-dma event-raw=");
		writeUnsigned(context, fault.eventRaw);
		context.write(" event-irq=");
		writeUnsigned(context, fault.eventIrq);
		context.write(" debug-status=");
		writeUnsigned(context, fault.debugStatus);
		context.write(" spi-status=");
		writeUnsigned(context, fault.spiStatus);
		context.write(" spi-dmacr=");
		writeUnsigned(context, fault.spiDmaControl);
		context.write(" spi-txflr=");
		writeUnsigned(context, fault.spiTxLevel);
		context.writeLine("");
	}
#endif

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
	    .usage = "display-test [name] [--bench] [--lcd-dma] [--lcd-direct16|--lcd-packed8] [--matrix] [--rows-scan] [--transport row|stream|double|cached|native|psram|all] [--size full|gb2|gb240|gb1|custom] [--baud <hz>] [--frames <count>] [--rows <count>] [--width <px>] [--height <px>]",
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

#if defined(LEXXOS_PLATFORM_RK3506)
	bool const useLcdDma = arguments.has("lcd-dma");
	bool const useDirect16 = arguments.has("lcd-direct16");
	bool const usePacked8 = arguments.has("lcd-packed8");

	if (!display->waitIdle()) {
		return 1;
	}

	bool const previousDma = lexxos::platform::rk3506::lcdDmaEnabled();
	bool const previousDirect16 = lexxos::platform::rk3506::lcdSpi0Rgb565Direct16Enabled();

	if (useDirect16 && usePacked8) {
		context.writeErrorLine("display-test: --lcd-direct16 and --lcd-packed8 are mutually exclusive");
		return 2;
	}

	if (usePacked8 && !options.bench) {
		context.writeErrorLine("display-test: --lcd-packed8 requires --bench");
		return 2;
	}

	if (useDirect16 && (!useLcdDma || !options.bench || display->wireFormat() != lexxos::devices::DisplayWireFormat::Rgb565)) {
		context.writeErrorLine("display-test: --lcd-direct16 requires --bench --lcd-dma and RGB565 wire format");
		return 2;
	}

	if (useLcdDma && !options.bench) {
		context.writeErrorLine("display-test: --lcd-dma requires --bench");
		return 2;
	}

	if (useLcdDma) {
		auto const enabled = [] {
			lexxos::devices::BusTransaction transaction{lexxos::composition::products::lyraSpi0Queue()};
			return transaction && lexxos::platform::rk3506::lcdDmaProbe() && lexxos::platform::rk3506::lcdDmaSetEnabled(true);
		}();
		if (!enabled) {
			context.write("display-test: SPI0 busy or DMA unavailable: ");
			context.writeLine(lexxos::platform::rk3506::lcdDmaReason());
			writeLcdDmaFailure(context);
			return 1;
		}

		context.writeLine("display-test: LCD DMA enabled (DMAC0 ch2, IRQ148 event2)");
	}

	if (useDirect16 || usePacked8) {
		bool const enabled = [useDirect16] {
			lexxos::devices::BusTransaction transaction{lexxos::composition::products::lyraSpi0Queue()};
			return transaction && lexxos::platform::rk3506::lcdSpi0SetRgb565Direct16(useDirect16);
		}();

		if (!enabled) {
			lexxos::devices::BusTransaction transaction{lexxos::composition::products::lyraSpi0Queue()};
			if (transaction) {
				(void) lexxos::platform::rk3506::lcdDmaSetEnabled(previousDma);
			}

			context.writeErrorLine("display-test: cannot select LCD pixel transport (DMA unavailable or SPI0 busy)");
			return 1;
		}

		context.writeLine(useDirect16
		        ? "display-test: RGB565 direct16 active (SPI0 DFS16 + PL330 halfwords)"
		        : "display-test: RGB565 packed8 active for this benchmark");
	}
#endif

	if (options.bench) {
#if defined(LEXXOS_PLATFORM_RK3506)
		auto const dmaBefore = lexxos::platform::rk3506::lcdDmaStatistics();
#endif
		auto result = runBenchmarkMode(context, *display, options);
#if defined(LEXXOS_PLATFORM_RK3506)
		if (useDirect16 || usePacked8) {
			bool const idle = display->waitIdle();
			bool const restored = [idle, previousDirect16] {
				if (!idle) {
					return false;
				}

				lexxos::devices::BusTransaction transaction{lexxos::composition::products::lyraSpi0Queue()};
				return transaction && lexxos::platform::rk3506::lcdSpi0SetRgb565Direct16(previousDirect16);
			}();

			if (!restored) {
				context.writeErrorLine("display-test: cannot restore default LCD pixel transport; reboot before further LCD use");
				result = 1;
			}
		}

		if (useLcdDma) {
			(void) display->waitIdle();

			auto const disabled = [previousDma] {
				lexxos::devices::BusTransaction transaction{lexxos::composition::products::lyraSpi0Queue()};
				return transaction && lexxos::platform::rk3506::lcdDmaSetEnabled(previousDma);
			}();

			if (!disabled) {
				context.writeErrorLine("display-test: cannot restore LCD DMA mode (SPI0 busy or DMA failure)");
				return 1;
			}

			auto stats = lexxos::platform::rk3506::lcdDmaStatistics();
			stats.chunks -= dmaBefore.chunks;
			stats.direct16Chunks -= dmaBefore.direct16Chunks;
			stats.bytes -= dmaBefore.bytes;
			stats.waitTicks -= dmaBefore.waitTicks;
			stats.irqTicks -= dmaBefore.irqTicks;
			stats.wakeupTicks -= dmaBefore.wakeupTicks;
			stats.drainTicks -= dmaBefore.drainTicks;
			stats.timedIrqs -= dmaBefore.timedIrqs;
			stats.errors -= dmaBefore.errors;
			stats.timeouts -= dmaBefore.timeouts;
			stats.irqInterrupts -= dmaBefore.irqInterrupts;
			stats.irqPollFallbacks -= dmaBefore.irqPollFallbacks;

			context.write("display-test: lcd-dma irq=");
			writeUnsigned(context, stats.irqInterrupts);
			context.write(" poll-fallbacks=");
			writeUnsigned(context, stats.irqPollFallbacks);
			context.writeLine("");
			context.write("display-test: lcd-dma chunks=");
			writeUnsigned(context, stats.chunks);
			context.write(" direct16-chunks=");
			writeUnsigned(context, stats.direct16Chunks);
			context.write(" bytes=");
			writeUnsigned64(context, stats.bytes);
			context.write(" wait-ticks=");
			writeUnsigned64(context, stats.waitTicks);
			context.write(" irq-ticks=");
			writeUnsigned64(context, stats.irqTicks);
			context.write(" wakeup-ticks=");
			writeUnsigned64(context, stats.wakeupTicks);
			context.write(" drain-ticks=");
			writeUnsigned64(context, stats.drainTicks);
			context.write(" timed-irqs=");
			writeUnsigned(context, stats.timedIrqs);
			context.write(" errors=");
			writeUnsigned(context, stats.errors);
			context.write(" timeouts=");
			writeUnsigned(context, stats.timeouts);
			context.write(" state=");
			context.writeLine(lexxos::platform::rk3506::lcdDmaReason());
			auto const regs = lexxos::platform::rk3506::lcdDmaRegisterSnapshot();
			context.write("display-test: spi0-clksel=");
			writeUnsigned(context, regs.spiClockSelect);
			context.write(" spi0-baud-divider=");
			writeUnsigned(context, regs.spiBaudDivider);
			context.write(" dmac0-ch2=");
			writeUnsigned(context, regs.dmaChannel2State);
			context.write(" dmac0-faults=");
			writeUnsigned(context, regs.dmaChannelFaults);
			context.write(" grf-dma-route=");
			writeUnsigned(context, regs.grfDmaRoute);
			context.writeLine("");
		}
#endif

		return result;
	}

	return runPatternMode(context, *display);
}

} // namespace lexxos::apps::device_programs
