/* Диагностическая матрица SD RK3506 удерживает SPI1 до восстановления настроек.
 * Raw-чтения используют существующее владение очередью без повторного захвата.
 * Для переносимых проверок файловой системы используются sd-bench и fs-smoke.
 */
#include <lexxos/apps/storage/rk3506/sd_hardware_bench_program.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/composition/products/lyra_storage_service.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/rk3506/picosd_diagnostic_session.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_dma_stress_policy.h>
#include <lexxos/devices/rk3506/picosd_dma_wait_profile.h>
#include <lexxos/devices/rk3506/picosd_profile_ro.h>
#include <lexxos/devices/rk3506/picosd_spi1_hw.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>
#include <lexxos/vfs/vfs.h>

#if defined(LEXXOS_RK3506_PMU_PROFILING)
	#include <lexxos/platform/rockchip/rk3506/pmu_profile.h>
#endif

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>
#endif

namespace lexxos::apps::storage_programs {

namespace {

	constexpr std::uint32_t kSectorSize = 512;
	constexpr std::size_t kBatchBlocks = 8;
	extern "C" std::uint32_t rk3506_timer_ticks();
	extern "C" std::uint64_t rk3506_timer_counter();
	extern "C" std::uint32_t rk3506_timer_counter_frequency();

#if defined(LEXXOS_RK3506_PMU_PROFILING)
	void printPmuDelta(lexxos::os::AppContext& ctx, char const* mode,
	    lexxos::platform::rk3506::PmuSample const& before,
	    lexxos::platform::rk3506::PmuSample const& after) {
		auto const delta = lexxos::platform::rk3506::pmuDifference(before, after);
		ctx.write("sd-hw-bench: Cortex-A7 PMU ");
		ctx.write(mode);
		ctx.write(": ");
		if (!delta.valid) {
			ctx.writeLine("unavailable (counter overflow or incomplete sample)");
			return;
		}

		ctx.write("cycles/instructions: ");
		lexxos::apps::writeUnsigned(ctx, delta.cycles);
		ctx.write("/");
		lexxos::apps::writeUnsigned(ctx, delta.instructions);
		ctx.writeLine(" (CPU0; includes SD CRC, hash, IRQ and diagnostic output)");
	}
#endif

	std::uint32_t hashBuffer(std::uint32_t hash, std::uint8_t const* bytes, std::size_t size) {
		for (std::size_t i = 0; i < size; ++i) {
			hash = (hash ^ bytes[i]) * 16777619U;
		}
		return hash;
	}

	void report(lexxos::os::AppContext& ctx, char const* name,
	    std::uint64_t bytes, std::uint32_t elapsed, std::uint32_t hash) {
		ctx.write("sd-hw-bench: ");
		ctx.write(name);
		ctx.write(" ");
		lexxos::apps::writeUnsigned64(ctx, bytes);
		ctx.write(" bytes in ");
		lexxos::apps::writeUnsigned(ctx, elapsed);
		ctx.write(" ms, ");
		lexxos::apps::writeUnsigned64(ctx, bytes * 1000ULL / (static_cast<std::uint64_t>(elapsed) * 1024U));
		ctx.write(" KiB/s; fnv1a=");
		lexxos::apps::writeHex32(ctx, hash);
		ctx.writeLine("");
	}

	bool rawRead(lexxos::os::AppContext& ctx, lexxos::devices::rk3506::PicoSdDiagnosticSession& session,
	    std::uint32_t blocks, std::uint32_t batch, std::uint8_t* buffer,
	    std::uint32_t& hashOut) {
		auto const before = lexxos::platform::rk3506::picoSpi1FifoStatistics();
		auto const dmaBefore = lexxos::platform::rk3506::picoSdDmaStatistics();
		auto const profileBefore = lexxos::platform::rk3506::picoSdReadProfileStatistics();

		std::uint64_t hashCycles = 0;
		std::uint32_t hash = 2166136261U;
		std::uint32_t const start = rk3506_timer_ticks();
		std::uint64_t const startCycles = rk3506_timer_counter();

		for (std::uint32_t first = 0; first < blocks;) {
			if (ctx.cancellationRequested()) {
				ctx.writeLine("sd-hw-bench: cancelled");
				return false;
			}

			std::uint32_t count = std::min(batch, blocks - first);
			if (!session.read(first, buffer, count)) {
				ctx.write("sd-hw-bench: raw read failed at LBA ");
				lexxos::apps::writeUnsigned(ctx, first);
				ctx.writeLine("");
				return false;
			}

			auto const hashStarted = rk3506_timer_counter();
			hash = hashBuffer(hash, buffer, count * kSectorSize);
			hashCycles += rk3506_timer_counter() - hashStarted;

			first += count;
		}

		std::uint64_t const elapsedCycles = rk3506_timer_counter() - startCycles;
		auto const elapsed = std::max<std::uint32_t>(1U, std::uint32_t(rk3506_timer_ticks() - start));

		auto const bytes = std::uint64_t(blocks) * kSectorSize;
		report(ctx, batch == 1 ? "raw CMD17" : "raw CMD18", bytes, elapsed, hash);
		// Both speeds use the same CNTVCT timer. Verified includes FNV1a;
		// read excludes only the FNV1a work, not CRC16 or SD protocol overhead.
		auto const hz = rk3506_timer_counter_frequency();
		auto const readCycles = elapsedCycles > hashCycles ? elapsedCycles - hashCycles : 0ULL;

		if (hz != 0U && readCycles != 0U && elapsedCycles != 0U) {
			ctx.write("sd-hw-bench: throughput KiB/s read/verified: ");
			lexxos::apps::writeUnsigned64(ctx, (bytes * hz) / (readCycles * 1024ULL));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, (bytes * hz) / (elapsedCycles * 1024ULL));
			ctx.writeLine(" (read excludes FNV1a; both verify SD CRC16)");
		}

		auto const after = lexxos::platform::rk3506::picoSpi1FifoStatistics();
		auto const dmaAfter = lexxos::platform::rk3506::picoSdDmaStatistics();
		if (dmaAfter.reads != dmaBefore.reads) {
			ctx.write("sd-hw-bench: REAL DMAC0 reads/bytes: ");
			lexxos::apps::writeUnsigned(ctx, dmaAfter.reads - dmaBefore.reads);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, dmaAfter.bytes - dmaBefore.bytes);
			ctx.writeLine("");
		}

		if (after.reads != before.reads) {
			ctx.write("sd-hw-bench: SPI1 FIFO ");
			ctx.write(lexxos::platform::rk3506::picoSpi1FifoMode() == lexxos::platform::rk3506::PicoSpi1FifoMode::Fast ? "fast" : "safe");
			ctx.write("; payloads=");
			lexxos::apps::writeUnsigned(ctx, after.reads - before.reads);
			ctx.write("; RX polls/empty=");
			lexxos::apps::writeUnsigned(ctx, after.rxPolls - before.rxPolls);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, after.emptyRxPolls - before.emptyRxPolls);
			ctx.write("; TX bursts/level checks=");
			lexxos::apps::writeUnsigned(ctx, after.txBursts - before.txBursts);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, after.txLevelChecks - before.txLevelChecks);
			ctx.write("; IRQ checks=");
			lexxos::apps::writeUnsigned(ctx, after.irqChecks - before.irqChecks);
			ctx.writeLine("");

			// CNTVCT is 24 MHz on RK3506; accumulate all 512-byte FIFO
			// transfers instead of rounding each one to 1 ms.
			std::uint32_t const hz = rk3506_timer_counter_frequency();
			if (hz != 0U) {
				std::uint64_t const totalUs = (elapsedCycles * 1000000ULL) / hz;
				std::uint64_t const payloadUs = ((after.payloadCycles - before.payloadCycles) * 1000000ULL) / hz;
				ctx.write("sd-hw-bench: time us total/FIFO/rest: ");
				lexxos::apps::writeUnsigned64(ctx, totalUs);
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, payloadUs);
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, totalUs >= payloadUs ? totalUs - payloadUs : 0ULL);
				ctx.writeLine(" (rest includes SD commands, CRC16 and benchmark hashing)");
			}
		}

		auto const p = lexxos::platform::rk3506::picoSdReadProfileStatistics();
		if (hz != 0U) {
			auto us = [hz](std::uint64_t cycles) -> std::uint64_t {
				return cycles * 1000000ULL / hz;
			};
			auto const cmd = p.commandCycles - profileBefore.commandCycles;
			auto const token = p.tokenCycles - profileBefore.tokenCycles;
			auto const crcWire = p.crcWireCycles - profileBefore.crcWireCycles;
			auto const crcMath = p.crcComputeCycles - profileBefore.crcComputeCycles;
			auto const stop = p.stopCycles - profileBefore.stopCycles;
			auto const fifo = after.payloadCycles - before.payloadCycles;
			auto const dma = dmaAfter.payloadCycles - dmaBefore.payloadCycles;
			auto const accounted = cmd + token + crcWire + crcMath + stop + fifo + dma + hashCycles;

			if (dma != 0U) {
				ctx.write("sd-hw-bench: DMA payload us: ");
				lexxos::apps::writeUnsigned64(ctx, us(dma));
				ctx.writeLine("");

				ctx.write("sd-hw-bench: DMA phases us setup/launch/wait/finish/copy: ");
				lexxos::apps::writeUnsigned64(ctx, us(dmaAfter.setupCycles - dmaBefore.setupCycles));
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, us(dmaAfter.launchCycles - dmaBefore.launchCycles));
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, us(dmaAfter.waitCycles - dmaBefore.waitCycles));
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, us(dmaAfter.finishCycles - dmaBefore.finishCycles));
				ctx.write("/");
				lexxos::apps::writeUnsigned64(ctx, us(dmaAfter.copyCycles - dmaBefore.copyCycles));
				ctx.writeLine("");
			}

			ctx.write("sd-hw-bench: phase us cmd/token/PIO-FIFO/CRCwire/CRCmath/CMD12/hash/other: ");

			lexxos::apps::writeUnsigned64(ctx, us(cmd));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(token));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(fifo));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(crcWire));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(crcMath));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(stop));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(hashCycles));
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, us(elapsedCycles >= accounted ? elapsedCycles - accounted : 0ULL));
			ctx.writeLine("");
			ctx.write("sd-hw-bench: counters CMD17/CMD18/CMD12/token-polls/good/CRCerr/token-err/read-err: ");
			lexxos::apps::writeUnsigned(ctx, p.cmd17 - profileBefore.cmd17);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.cmd18 - profileBefore.cmd18);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.cmd12 - profileBefore.cmd12);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.tokenPolls - profileBefore.tokenPolls);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.goodSectors - profileBefore.goodSectors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.crcErrors - profileBefore.crcErrors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.tokenErrors - profileBefore.tokenErrors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, p.readErrors - profileBefore.readErrors);
			ctx.writeLine("");
		}

		hashOut = hash;
		return true;
	}

	// PIO matrix is always the reference. DMA runs separately only when
	// explicitly opted in with `sd-diag dma probe; sd-diag dma on`.
	bool runMatrixPass(lexxos::os::AppContext& ctx, lexxos::devices::rk3506::PicoSdDiagnosticSession& session,
	    std::uint8_t* buffer, std::uint32_t& baseline128, std::uint32_t& baseline1024) {
		auto const originalHz = lexxos::platform::rk3506::picoSpi1ClockMHz();
		auto const originalMode = lexxos::platform::rk3506::picoSpi1FifoMode();
		if (originalHz != 6U && originalHz != 12U) {
			ctx.writeLine("sd-hw-bench: matrix requires initialized hardware SPI1");
			return false;
		}

		struct Restore {
			lexxos::os::AppContext& out;
			std::uint32_t clock;
			lexxos::platform::rk3506::PicoSpi1FifoMode mode;

			~Restore() {
				bool const modeOk = lexxos::platform::rk3506::picoSpi1SetFifoMode(mode);
				bool const clockOk = lexxos::platform::rk3506::picoSpi1SetClockMHz(clock);
				if (!modeOk || !clockOk) {
					out.writeLine("sd-hw-bench: WARNING restore failed; inspect sd-diag status");
				}
			}
		} restore{ctx, originalHz, originalMode};

		for (std::uint32_t const mhz : {6U, 12U}) {
			if (!lexxos::platform::rk3506::picoSpi1SetClockMHz(mhz)) {
				ctx.writeLine("sd-hw-bench: matrix clock change failed");
				return false;
			}

			for (auto const mode : {lexxos::platform::rk3506::PicoSpi1FifoMode::Safe, lexxos::platform::rk3506::PicoSpi1FifoMode::Fast}) {
				if (!lexxos::platform::rk3506::picoSpi1SetFifoMode(mode)) {
					ctx.writeLine("sd-hw-bench: matrix FIFO mode change failed");
					return false;
				}

				for (std::uint32_t const blocks : {128U, 1024U}) {
					ctx.write("sd-hw-bench: matrix PIO ");
					lexxos::apps::writeUnsigned(ctx, mhz);
					ctx.write(" MHz/");
					ctx.write(mode == lexxos::platform::rk3506::PicoSpi1FifoMode::Safe ? "safe" : "fast");
					ctx.write(" blocks=");
					lexxos::apps::writeUnsigned(ctx, blocks);
					ctx.writeLine("");
					std::uint32_t h1 = 0U, h8 = 0U;
					if (!rawRead(ctx, session, blocks, 1U, buffer, h1)
					    || !rawRead(ctx, session, blocks, kBatchBlocks, buffer, h8)) {
						return false;
					}

					if (h1 != h8) {
						ctx.writeLine("sd-hw-bench: ERROR CMD17/CMD18 hash mismatch");
						return false;
					}

					auto& baseline = blocks == 128U ? baseline128 : baseline1024;
					if (baseline == 0U) {
						baseline = h1;
					} else if (baseline != h1) {
						ctx.writeLine("sd-hw-bench: ERROR cross-mode/hash mismatch");
						return false;
					}
				}
			}

			auto const dma = lexxos::platform::rk3506::picoSpi1DmaInfo();
			if (!dma.available) {
				ctx.write("sd-hw-bench: matrix DMA ");
				lexxos::apps::writeUnsigned(ctx, mhz);
				ctx.write(" MHz: SKIP - ");
				ctx.writeLine(dma.reason);
			} else {
				ctx.writeLine("sd-hw-bench: DMA deferred until full PIO baseline completes");
			}
		}

		ctx.writeLine("sd-hw-bench: matrix CRC pass OK (PIO only; DMA not measured)");
		return true;
	}

	// Eight real DMA reads, using the already verified PIO hashes as the
	// content baseline. Only callable following explicit DMA opt-in.
	bool runDmaMatrix(lexxos::os::AppContext& ctx, lexxos::devices::rk3506::PicoSdDiagnosticSession& session,
	    std::uint8_t* buffer, std::uint32_t baseline128, std::uint32_t baseline1024) {
		auto const originalHz = lexxos::platform::rk3506::picoSpi1ClockMHz();

		struct Restore {
			lexxos::os::AppContext& out;
			std::uint32_t hz;

			~Restore() {
				(void) lexxos::platform::rk3506::picoSdDmaSetEnabled(false);
				if (!lexxos::platform::rk3506::picoSpi1SetClockMHz(hz)) {
					out.writeLine("sd-hw-bench: WARNING SPI restore failed");
				}
			}
		} restore{ctx, originalHz};

		if (!lexxos::platform::rk3506::picoSdDmaSetEnabled(true)) {
			return false;
		}

		for (std::uint32_t mhz : {6U, 12U}) {
			if (!lexxos::platform::rk3506::picoSpi1SetClockMHz(mhz)) {
				return false;
			}

			for (std::uint32_t blocks : {128U, 1024U}) {
				ctx.write("sd-hw-bench: matrix REAL DMA ");
				lexxos::apps::writeUnsigned(ctx, mhz);
				ctx.write(" MHz blocks=");
				lexxos::apps::writeUnsigned(ctx, blocks);
				ctx.writeLine("");

				std::uint32_t singleHash = 0U, multiHash = 0U;
				if (!rawRead(ctx, session, blocks, 1U, buffer, singleHash) || !rawRead(ctx, session, blocks, kBatchBlocks, buffer, multiHash)) {
					return false;
				}

				if (singleHash != multiHash || singleHash != (blocks == 128U ? baseline128 : baseline1024)) {
					ctx.writeLine("sd-hw-bench: ERROR DMA/PIO data mismatch");
					return false;
				}
			}
		}
		ctx.writeLine("sd-hw-bench: real DMA/PIO hashes match");
		return true;
	}

	// Run the 16-read PIO matrix with all three CRC algorithms. Baseline
	// hashes are shared across modes: a mismatch stops the test immediately.
	bool runMatrix(lexxos::os::AppContext& ctx, lexxos::devices::rk3506::PicoSdDiagnosticSession& session,
	    std::uint8_t* buffer) {
		bool const dmaRequested = lexxos::platform::rk3506::picoSdDmaEnabled();
		if (dmaRequested && !lexxos::platform::rk3506::picoSdDmaSetEnabled(false)) {
			return false;
		}

		struct RestoreDma {
			lexxos::os::AppContext& out;
			bool requested;

			~RestoreDma() {
				if (requested && !lexxos::platform::rk3506::picoSdDmaSetEnabled(true)) {
					out.writeLine("sd-hw-bench: WARNING experimental DMA not restored");
				}
			}
		} restoreDma{ctx, dmaRequested};

		struct RestoreCrc {
			lexxos::os::AppContext& out;
			lexxos::platform::rk3506::PicoSdCrcMode saved;

			~RestoreCrc() {
				if (!lexxos::platform::rk3506::picoSdSetCrcMode(saved)) {
					out.writeLine("sd-hw-bench: WARNING CRC mode restore failed");
				}
			}
		} restore{ctx, lexxos::platform::rk3506::picoSdCrcMode()};

		std::uint32_t baseline128 = 0U, baseline1024 = 0U;
		for (auto const mode : {lexxos::platform::rk3506::PicoSdCrcMode::Bitwise, lexxos::platform::rk3506::PicoSdCrcMode::Table, lexxos::platform::rk3506::PicoSdCrcMode::Slicing4}) {
			if (!lexxos::platform::rk3506::picoSdSetCrcMode(mode)) {
				ctx.writeLine("sd-hw-bench: matrix CRC mode change failed");
				return false;
			}

			ctx.write("sd-hw-bench: matrix CRC ");
			ctx.writeLine(mode == lexxos::platform::rk3506::PicoSdCrcMode::Slicing4 ? "slice4" : mode == lexxos::platform::rk3506::PicoSdCrcMode::Table ? "table"
			                                                                                                                                            : "bitwise");
			if (!runMatrixPass(ctx, session, buffer, baseline128, baseline1024)) {
				return false;
			}
		}

		if (dmaRequested) {
			if (!lexxos::platform::rk3506::picoSdSetCrcMode(lexxos::platform::rk3506::PicoSdCrcMode::Slicing4) || !runDmaMatrix(ctx, session, buffer, baseline128, baseline1024)) {
				ctx.writeLine("sd-hw-bench: real DMA test FAILED (reprobe before retry)");
				return false;
			}

			ctx.writeLine("sd-hw-bench: matrix OK (48 PIO + 8 REAL DMA reads; hashes match)");
		} else {
			ctx.writeLine("sd-hw-bench: matrix OK (48 PIO reads; DMA SKIP until explicit opt-in)");
		}

		return true;
	}

	// Read-only benchmark and mode-switch test. The caller already holds the
	// SPI1 diagnostic queue and the exclusive sd1 resource lock.
	bool runDmaStress(lexxos::os::AppContext& ctx,
	    lexxos::devices::rk3506::PicoSdDiagnosticSession& session,
	    std::uint8_t* buffer, std::uint32_t rounds, bool includeEvent) {
		using lexxos::platform::rk3506::picoSdDmaDirect;
		using lexxos::platform::rk3506::picoSdDmaEnabled;
		using lexxos::platform::rk3506::picoSdDmaSetDirect;
		using lexxos::platform::rk3506::picoSdDmaSetEnabled;
		using lexxos::platform::rk3506::picoSdDmaSetWaitEvent;
		using lexxos::platform::rk3506::picoSdDmaSetWaitIrq;
		using lexxos::platform::rk3506::picoSdDmaStatistics;
		using lexxos::platform::rk3506::picoSdDmaWaitEvent;
		using lexxos::platform::rk3506::picoSdDmaWaitIrq;
		using lexxos::platform::rk3506::picoSpi1DmaInfo;
#if defined(LEXXOS_RK3506_PMU_PROFILING)
		using lexxos::platform::rk3506::pmuBusyCycleEnabled;
		using lexxos::platform::rk3506::pmuBusyCycleInterrupt;
		using lexxos::platform::rk3506::pmuBusyCycleOverflow;
		using lexxos::platform::rk3506::pmuBusyEvent0Enabled;
		using lexxos::platform::rk3506::pmuBusyEvent0Interrupt;
		using lexxos::platform::rk3506::pmuBusyEvent0Overflow;
		using lexxos::platform::rk3506::pmuBusyGlobalEnable;
		using lexxos::platform::rk3506::pmuBusyReasons;
		using lexxos::platform::rk3506::pmuRegisterSnapshot;
		using lexxos::platform::rk3506::PmuSample;
		using lexxos::platform::rk3506::pmuSample;
		using lexxos::platform::rk3506::pmuStart;
		using lexxos::platform::rk3506::pmuStatus;
		using lexxos::platform::rk3506::pmuStop;
#endif
#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
		using lexxos::platform::rk3506::GicIrqSnapshot;
		using lexxos::platform::rk3506::rk3506_gic_irq_delivery_count;
		using lexxos::platform::rk3506::rk3506_gic_irq_snapshot;
#endif

		if (!picoSdDmaEnabled() || !picoSpi1DmaInfo().available) {
			ctx.writeLine("sd-hw-bench: first run sd-diag dma probe; sd-diag dma on");
			return false;
		}

		struct Restore {
			lexxos::os::AppContext& ctx;
			bool dma, direct, irq, event;
			bool healthy{true};

			~Restore() {
				// Stop the test even when a read failed. Never reprobe or restart
				// a faulted channel behind the SD driver's back.
				bool const waitOff = picoSdDmaSetWaitIrq(false);
				bool const dmaOff = picoSdDmaSetEnabled(false);
				if (!healthy) {
					ctx.writeLine("sd-hw-bench: DMA left OFF after failed stress; inspect/reprobe before retry");
					if (!waitOff || !dmaOff) {
						ctx.writeLine("sd-hw-bench: WARNING wait-mode release incomplete");
					}

					return;
				}

				bool const directOk = picoSdDmaSetDirect(direct);
				bool const waitOk = irq ? picoSdDmaSetWaitIrq(true)
				    : event             ? picoSdDmaSetWaitEvent(true)
				                        : true;

				bool const dmaOk = !dma || picoSdDmaSetEnabled(true);
				if (!waitOff || !dmaOff || !directOk || !waitOk || !dmaOk) {
					ctx.writeLine("sd-hw-bench: WARNING DMA restore incomplete; check sd-diag dma status / reprobe");
				}
			}
		} restore{ctx, picoSdDmaEnabled(), picoSdDmaDirect(), picoSdDmaWaitIrq(), picoSdDmaWaitEvent()};

#if defined(LEXXOS_RK3506_PMU_PROFILING)
		struct StopPmu {
			~StopPmu() { pmuStop(); }
		} stopPmu{};

		bool const pmuReady = pmuStart();
		ctx.write("sd-hw-bench: Cortex-A7 PMU: ");
		ctx.writeLine(pmuStatus());
		if (!pmuReady) {
			auto const snapshot = pmuRegisterSnapshot();
			if (snapshot.valid) {
				ctx.write("sd-hw-bench: PMU PMCR/PMCNTENSET/PMINTENSET/PMOVSR/PMSELR: ");
				lexxos::apps::writeHex32(ctx, snapshot.pmcr);
				ctx.write(" / ");
				lexxos::apps::writeHex32(ctx, snapshot.enabled);
				ctx.write(" / ");
				lexxos::apps::writeHex32(ctx, snapshot.interruptEnabled);
				ctx.write(" / ");
				lexxos::apps::writeHex32(ctx, snapshot.overflow);
				ctx.write(" / ");
				lexxos::apps::writeHex32(ctx, snapshot.selector);
				ctx.writeLine("");

				auto const reasons = pmuBusyReasons(snapshot);
				ctx.write("sd-hw-bench: PMU state flags (PMCR.E alone is not busy):");
				if (reasons == 0U) {
					ctx.write(" none");
				}

				auto const printReason = [&](std::uint32_t flag, char const* description) {
					if ((reasons & flag) != 0U) {
						ctx.write(" ");
						ctx.write(description);
					}
				};

				printReason(pmuBusyGlobalEnable, "PMCR.E(global-only)");
				printReason(pmuBusyCycleEnabled, "PMCNTENSET.cycle");
				printReason(pmuBusyEvent0Enabled, "PMCNTENSET.event0");
				printReason(pmuBusyCycleInterrupt, "PMINTENSET.cycle");
				printReason(pmuBusyEvent0Interrupt, "PMINTENSET.event0");
				printReason(pmuBusyCycleOverflow, "PMOVSR.cycle(stale-possible)");
				printReason(pmuBusyEvent0Overflow, "PMOVSR.event0(stale-possible)");

				ctx.writeLine("");
			}
		}
#else
		ctx.writeLine("sd-hw-bench: Cortex-A7 PMU disabled at build time");
#endif

		if (!picoSdDmaSetEnabled(false) || !picoSdDmaSetWaitIrq(false) || !picoSdDmaSetDirect(false)) {
			restore.healthy = false;
			ctx.writeLine("sd-hw-bench: cannot switch DMA safely to buffered PIO baseline");
			return false;
		}

		ctx.writeLine("sd-hw-bench: stress PIO reference (128 sectors, LBA 0..127)");
		std::uint32_t baseline{};

#if defined(LEXXOS_RK3506_PMU_PROFILING)
		auto const pioBefore = pmuReady ? pmuSample() : PmuSample{};
#endif
		bool const pioOk = rawRead(ctx, session, 128U, 1U, buffer, baseline);
#if defined(LEXXOS_RK3506_PMU_PROFILING)
		auto const pioAfter = pmuReady ? pmuSample() : PmuSample{};
		if (pmuReady) {
			printPmuDelta(ctx, "PIO reference", pioBefore, pioAfter);
		}
#endif
		if (!pioOk) {
			restore.healthy = false;
			return false;
		}

		if (!picoSdDmaSetEnabled(true)) {
			restore.healthy = false;
			ctx.writeLine("sd-hw-bench: DMA re-enable refused after PIO baseline");
			return false;
		}

		for (std::uint32_t round = 0U; round < rounds; ++round) {
			for (std::uint32_t mode = 0U; mode < (includeEvent ? 3U : 2U); ++mode) {
				if (ctx.cancellationRequested()) {
					ctx.writeLine("sd-hw-bench: stress cancelled");
					return false;
				}

				// Every transition passes through the polling state, which
				// releases only our IRQ148 subscription or CNTKCTL event fields.
				if (!picoSdDmaSetWaitIrq(false)
				    || (mode == 1U && !picoSdDmaSetWaitIrq(true))
				    || (mode == 2U && !picoSdDmaSetWaitEvent(true))) {
					restore.healthy = false;
					ctx.writeLine("sd-hw-bench: stress wait-mode transition refused");
					return false;
				}

				ctx.write("sd-hw-bench: stress round ");
				lexxos::apps::writeUnsigned(ctx, round + 1U);
				ctx.write(" mode=");
				ctx.writeLine(mode == 0U ? "poll" : mode == 1U ? "irq"
				                                               : "event");

				auto const before = picoSdDmaStatistics();
#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
				std::uint32_t const irq149Before = rk3506_gic_irq_delivery_count(149U);
				GicIrqSnapshot irq149StateBefore{};
				bool const irq149SnapshotOk = rk3506_gic_irq_snapshot(149U, &irq149StateBefore);
#endif
				std::uint32_t actualHash{};

#if defined(LEXXOS_RK3506_PMU_PROFILING)
				auto const pmuBefore = pmuReady ? pmuSample() : PmuSample{};
#endif
				bool const readOk = rawRead(ctx, session, 128U, 1U, buffer, actualHash);
#if defined(LEXXOS_RK3506_PMU_PROFILING)
				auto const pmuAfter = pmuReady ? pmuSample() : PmuSample{};
				if (pmuReady) {
					printPmuDelta(ctx, mode == 0U ? "poll" : mode == 1U ? "irq"
					                                                    : "event",
					    pmuBefore, pmuAfter);
				}
#endif
				if (!readOk) {
					restore.healthy = false;
					if (!ctx.cancellationRequested()) {
						ctx.writeLine("sd-hw-bench: stress I/O error; abort and reprobe before retry");
					}

					return false;
				}

				auto const after = picoSdDmaStatistics();
				if (actualHash != baseline || !platform::rk3506::detail::dmaStressPassOk(before, after, 128U, mode == 1U)) {
					restore.healthy = false;
					ctx.writeLine("sd-hw-bench: stress FAILED (hash / DMA reads / IRQ / fault / fallback)");
					return false;
				}

				ctx.write("sd-hw-bench: stress DMA read/IRQ/RX/TX/both/IRQ-WFE/event-WFE/polls delta: ");
				lexxos::apps::writeUnsigned(ctx, after.reads - before.reads);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.irqInterrupts - before.irqInterrupts);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.irqRxEvents - before.irqRxEvents);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.irqTxEvents - before.irqTxEvents);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.irqCombinedEvents - before.irqCombinedEvents);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.irqSleeps - before.irqSleeps);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.eventSleeps - before.eventSleeps);
				ctx.write("/");
				lexxos::apps::writeUnsigned(ctx, after.waitPolls - before.waitPolls);
				ctx.writeLine("");

				auto const hz = rk3506_timer_counter_frequency();
				if (hz != 0U) {
					auto const elapsed = after.waitCycles - before.waitCycles;
					auto const irqWfe = after.irqWfeCycles - before.irqWfeCycles;
					auto const eventWfe = after.eventWfeCycles - before.eventWfeCycles;
					auto const nonWfe = platform::rk3506::detail::dmaNonWfeWindowCycles(elapsed, irqWfe, eventWfe);
					auto const us = [hz](std::uint64_t cycles) {
						return cycles * 1000000ULL / hz;
					};

					ctx.write("sd-hw-bench: stress DMA wait us total/IRQ-WFE/event-WFE/non-WFE: ");
					lexxos::apps::writeUnsigned64(ctx, us(elapsed));
					ctx.write("/");
					lexxos::apps::writeUnsigned64(ctx, us(irqWfe));
					ctx.write("/");
					lexxos::apps::writeUnsigned64(ctx, us(eventWfe));
					ctx.write("/");
					lexxos::apps::writeUnsigned64(ctx, us(nonWfe));
					ctx.writeLine(" (wall-time windows, not CPU load)");
				}

				ctx.write("sd-hw-bench: stress DMA wait loop checks delta: ");
				lexxos::apps::writeUnsigned(ctx, after.waitChecks - before.waitChecks);
				ctx.writeLine("");

#if defined(LEXXOS_RK3506_PMU_PROFILING)
				if (pmuReady) {
					ctx.write("sd-hw-bench: stress DMA wait PMU cycles/instructions/samples/invalid delta: ");
					lexxos::apps::writeUnsigned64(ctx, after.waitPmuCycles - before.waitPmuCycles);
					ctx.write("/");
					lexxos::apps::writeUnsigned64(ctx, after.waitPmuInstructions - before.waitPmuInstructions);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, after.waitPmuSamples - before.waitPmuSamples);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, after.waitPmuInvalid - before.waitPmuInvalid);
					ctx.writeLine("");
				}
#endif

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
				ctx.write("sd-hw-bench: IRQ149 GIC deliveries delta (READ ONLY; line never enabled): ");
				lexxos::apps::writeUnsigned(ctx, rk3506_gic_irq_delivery_count(149U) - irq149Before);
				ctx.writeLine("");
				GicIrqSnapshot irq149StateAfter{};
				if (irq149SnapshotOk && rk3506_gic_irq_snapshot(149U, &irq149StateAfter)) {
					ctx.write("sd-hw-bench: IRQ149 GIC EN/PD/AC before -> after (non-secure view): ");
					lexxos::apps::writeUnsigned(ctx, irq149StateBefore.enabled);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, irq149StateBefore.pending);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, irq149StateBefore.active);
					ctx.write(" -> ");
					lexxos::apps::writeUnsigned(ctx, irq149StateAfter.enabled);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, irq149StateAfter.pending);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, irq149StateAfter.active);
					ctx.writeLine("");
				}
#endif
			}
		}

		ctx.writeLine("sd-hw-bench: DMA stress OK; PIO hashes match all modes");
		return true;
	}

} // namespace

lexxos::program::ProgramDescriptor SdHardwareBenchProgram::descriptor() {
	return {
	    .name = "sd-hw-bench",
	    .help = "profile RK3506 SPI1 clock/FIFO/CRC/DMA matrix and read-only DMA stress",
	    .description = "Hardware maintenance matrix; portable tests are provided by sd-bench",
	    .usage = "sd-hw-bench [matrix | dma-stress [1..16 [event]]]",
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 16384,
	                .priority = 1,
	                .cancellable = true,
	                },
	    .allowExtraArguments = true,
	    .enabled = true,
	};
}

int SdHardwareBenchProgram::run(lexxos::os::AppContext& ctx,
    lexxos::program::ProgramInvocation const& inv,
    lexxos::program::ParsedArguments const&) {
	bool const stress = inv.argc >= 2 && std::string_view{inv.argv[1]} == "dma-stress";

	if ((!stress && (inv.argc > 2 || (inv.argc == 2 && std::string_view{inv.argv[1]} != "matrix")))
	    || (stress && inv.argc > 4)) {
		ctx.writeLine("usage: sd-hw-bench [matrix | dma-stress [1..16 [event]]]");
		return 1;
	}

	std::uint32_t rounds = 2U;
	bool includeEvent = false;
	if (stress && inv.argc >= 3) {
		std::string_view const digits{inv.argv[2]};
		if (digits.empty() || digits.size() > 2U) {
			ctx.writeLine("sd-hw-bench: rounds must be 1..16");
			return 1;
		}

		rounds = 0U;
		for (char const digit : digits) {
			if (digit < '0' || digit > '9') {
				ctx.writeLine("sd-hw-bench: rounds must be 1..16");
				return 1;
			}

			rounds = rounds * 10U + static_cast<std::uint32_t>(digit - '0');
		}

		if (rounds == 0U || rounds > 16U) {
			ctx.writeLine("sd-hw-bench: rounds must be 1..16");
			return 1;
		}
	}

	if (stress && inv.argc == 4) {
		if (std::string_view{inv.argv[3]} != "event") {
			ctx.writeLine("usage: sd-hw-bench dma-stress [1..16 [event]]");
			return 1;
		}
		includeEvent = true;
	}

	lexxos::devices::rk3506::PicoSdDiagnosticSession session{
	    lexxos::composition::products::lyraSpi1Queue(), lexxos::composition::products::lyraStorageTransport()};
	if (!session) {
		ctx.writeLine("sd-hw-bench: SPI1 busy");
		return 1;
	}

	auto* const device = ctx.vfs().blockDevice("sd1");
	if (device != &lexxos::composition::products::lyraStorageService().backend() || !device->isReady() || device->blockSizeBytes() != kSectorSize || device->blockCount() < 1024U) {
		ctx.writeLine("sd-hw-bench: matrix needs ready sd1 with >=1024 sectors");
		return 1;
	}

	auto lock = ctx.devices().lockBlockDevice("sd1", lexxos::devices::DeviceResourcePriority::Interactive, "sd-hw-bench");
	if (!lock) {
		ctx.writeLine("sd-hw-bench: sd1 is busy");
		return 1;
	}

	std::array<std::uint8_t, kSectorSize * kBatchBlocks> buffer{};
	bool const success = stress ? runDmaStress(ctx, session, buffer.data(), rounds, includeEvent)
	                            : runMatrix(ctx, session, buffer.data());

	return success ? 0 : ctx.cancellationRequested() ? 130
	                                                 : 1;
}

} // namespace lexxos::apps::storage_programs
