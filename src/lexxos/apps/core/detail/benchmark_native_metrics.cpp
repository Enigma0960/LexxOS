#include <lexxos/apps/core/detail/benchmark_probes.h>
#include <lexxos/feature.h>

#if defined(LEXXOS_PLATFORM_RK3506)
	#include <lexxos/composition/products/lyra_sd_dma.h>
	#include <lexxos/composition/products/lyra_sd_protocol.h>
	#include <lexxos/composition/products/lyra_spi_queues.h>
	#include <lexxos/devices/bus/bus_transaction.h>
	#include <lexxos/devices/rk3506/sd_dma_controller.h>
	#include <lexxos/devices/rk3506/sd_protocol.h>
	#if FEATURE_ENABLED(DISPLAY)
		#include <lexxos/app/picocalc_lcd_console.h>
		#include <lexxos/composition/products/lyra_lcd_dma.h>
		#include <lexxos/composition/products/lyra_lcd_spi.h>
		#include <lexxos/composition/products/picocalc_lyra_device_providers.h>
		#include <lexxos/devices/rk3506/lcd_dma_controller.h>
		#include <lexxos/devices/rk3506/lcd_spi_controller.h>
	#endif
#endif

namespace lexxos::apps::system_programs::detail {

void sampleBenchmarkNative(BenchmarkMetrics& report) {
#if defined(LEXXOS_PLATFORM_RK3506)
	namespace product = lexxos::composition::products;
	{
		lexxos::devices::BusTransaction lease{product::lyraSpi1Queue()};
		if (lease) {
			auto const profile = product::lyraSdProtocol().profileStatistics();
			auto const dma = product::lyraSdDma().statistics();
			auto const state = product::lyraSdProtocol().diagnosticStatus();
			report.add("sd-native", "stage", state.step, "", "cached; cumulative counters below");
			report.add("sd-native", "dma-enabled", product::lyraSdDma().enabled() ? "yes" : "no", "");
			report.addUnsigned("sd-native", "sectors", profile.goodSectors, "count");
			report.addUnsigned("sd-native", "read-errors", profile.readErrors, "count");
			report.addUnsigned("sd-native", "crc-errors", profile.crcErrors, "count");
			report.addUnsigned("sd-native", "token-errors", profile.tokenErrors, "count");
			report.addUnsigned("sd-native", "token-polls", profile.tokenPolls, "count");
			report.addUnsigned("sd-native", "command-ticks", profile.commandCycles, "ticks", "CNTVCT 24MHz");
			report.addUnsigned("sd-native", "token-ticks", profile.tokenCycles, "ticks", "CNTVCT 24MHz");
			report.addUnsigned("sd-native", "crc-ticks", profile.crcComputeCycles, "ticks", "CNTVCT 24MHz");
			report.addUnsigned("sd-dma", "reads", dma.reads, "count");
			report.addUnsigned("sd-dma", "bytes", dma.bytes, "bytes");
			report.addUnsigned("sd-dma", "errors", dma.errors, "count");
			report.addUnsigned("sd-dma", "timeouts", dma.timeouts, "count");
			report.addUnsigned("sd-dma", "wait-ticks", dma.waitCycles, "ticks");
			report.addUnsigned("sd-dma", "irq", dma.irqInterrupts, "count");
			report.addUnsigned("sd-dma", "poll-fallback", dma.irqPollFallbacks, "count");
		} else {
			report.add("sd-native", "snapshot", "skipped", "", "SPI1 busy");
		}
	}
	#if FEATURE_ENABLED(DISPLAY)
	{
		lexxos::devices::BusTransaction lease{product::lyraSpi0Queue()};
		if (lease) {
			auto const spi = product::lyraLcdSpi().statistics();
			auto const dma = product::lyraLcdDma().statistics();
			report.addUnsigned("lcd-spi", "calls", spi.calls, "count", "cumulative");
			report.addUnsigned("lcd-spi", "requested", spi.requestedBytes, "bytes");
			report.addUnsigned("lcd-spi", "completed", spi.completedBytes, "bytes");
			report.addUnsigned("lcd-spi", "failures", spi.failures, "count");
			report.addUnsigned("lcd-spi", "elapsed-ticks", spi.elapsedTicks, "ticks");
			report.addUnsigned("lcd-spi", "counter-hz", spi.counterHz, "Hz");
			report.add("lcd-dma", "enabled", product::lyraLcdDma().enabled() ? "yes" : "no", "");
			report.add("lcd-dma", "reason", product::lyraLcdDma().reason(), "");
			report.addUnsigned("lcd-dma", "chunks", dma.chunks, "count");
			report.addUnsigned("lcd-dma", "bytes", dma.bytes, "bytes");
			report.addUnsigned("lcd-dma", "errors", dma.errors, "count");
			report.addUnsigned("lcd-dma", "timeouts", dma.timeouts, "count");
			report.addUnsigned("lcd-dma", "wait-ticks", dma.waitTicks, "ticks");
			report.addUnsigned("lcd-dma", "wakeup-ticks", dma.wakeupTicks, "ticks");
			report.addUnsigned("lcd-dma", "drain-ticks", dma.drainTicks, "ticks");
		} else {
			report.add("lcd-native", "snapshot", "skipped", "", "SPI0 busy");
		}
	}

	auto const frame = product::picoCalcLyraDisplayTiming();
	report.addUnsigned("lcd-frame", "presented", frame.presentedFrames, "count", "cumulative");
	report.addUnsigned("lcd-frame", "draw-ticks", frame.drawTicks, "ticks", "includes scheduling; CNTVCT 24MHz");
	report.addUnsigned("lcd-frame", "copy-ticks", frame.copyTicks, "ticks");
	report.addUnsigned("lcd-frame", "queue-ticks", frame.queueTicks, "ticks");
	report.addUnsigned("lcd-frame", "present-ticks", frame.presentTicks, "ticks");
	auto const console = lexxos::app::picocalc_lcd_console::info();
	auto const render = lexxos::app::picocalc_lcd_console::renderStats();
	report.addUnsigned("lcd-console", "queued-chars", console.queueCount, "count");
	report.addUnsigned("lcd-console", "processed-chars", render.charactersProcessed, "count", "cumulative");
	report.addUnsigned("lcd-console", "flushes", render.flushCalls, "count");
	report.addUnsigned("lcd-console", "max-parse", render.maxParseMs, "ms", "since stats reset; includes scheduling");
	report.addUnsigned("lcd-console", "max-flush", render.maxFlushMs, "ms", "since stats reset; includes scheduling");
	report.addUnsigned("lcd-console", "drawn-rows", render.drawRowCalls, "count");
	#endif
#else
	report.add("native", "metrics", "n/a", "", "Lyra-only counters");
#endif
}

} // namespace lexxos::apps::system_programs::detail
