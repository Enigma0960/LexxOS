#include <lexxos/apps/storage/rk3506/picosd_program.h>

#include <lexxos/app/rk3506/picosd_storage.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/composition/products/lyra_storage_service.h>
#include <lexxos/devices/rk3506/picosd_diagnostic_session.h>
#include <lexxos/devices/rk3506/picosd_diagnostics.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_dma_wait_profile.h>
#include <lexxos/devices/rk3506/picosd_spi1_hw.h>
#include <lexxos/devices/rk3506/picosd_transport.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_manager.h>

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	#include <lexxos/platform/rockchip/rk3506/gic_irq_catalog.h>
	#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::size_t kSectorSize = 512;
	extern "C" std::uint32_t rk3506_timer_counter_frequency();

	void hex(lexxos::os::AppContext& ctx, std::uint32_t value) {
		// No string-literal-to-stack bulk copy or implicit strlen here.
		// Useful both for early UART diagnostics and optimized ARM builds.
		ctx.write(std::string_view{"0x", 2});
		for (unsigned i = 0; i < 8; ++i) {
			auto const nibble = static_cast<unsigned>((value >> (28U - i * 4U)) & 15U);
			char const digit = static_cast<char>(nibble < 10U ? ('0' + nibble) : ('a' + nibble - 10U));
			ctx.write(std::string_view{&digit, 1});
		}
	}

	void hexByte(lexxos::os::AppContext& ctx, std::uint8_t b) {
		constexpr char digits[] = "0123456789abcdef";
		char out[3]{digits[b >> 4U], digits[b & 15U], '\0'};
		ctx.write(out);
	}

	int run(lexxos::os::AppContext& ctx,
	    lexxos::program::ProgramInvocation const& inv,
	    lexxos::program::ParsedArguments const&) {
		lexxos::devices::rk3506::PicoSdDiagnosticSession session{
		    lexxos::composition::products::lyraSpi1Queue(), lexxos::composition::products::lyraStorageTransport()};
		if (!session) {
			ctx.writeLine("sd-diag: SPI1 busy");
			return 1;
		}

		auto const action = inv.argc >= 2 ? std::string_view(inv.argv[1]) : "status";

		if (action == "crc") {
			if (inv.argc != 3) {
				ctx.writeLine("usage: sd-diag crc [status | bitwise | table | slice4]");
				return 1;
			}

			auto const option = std::string_view(inv.argv[2]);
			if (option == "status") {
				ctx.write("sd-diag: CRC16 mode: ");
				ctx.writeLine(picoSdCrcMode() == PicoSdCrcMode::Slicing4 ? "slice4"
				        : picoSdCrcMode() == PicoSdCrcMode::Table        ? "table"
				                                                         : "bitwise");
				return 0;
			}

			if (option != "bitwise" && option != "table" && option != "slice4") {
				ctx.writeLine("usage: sd-diag crc [status | bitwise | table | slice4]");
				return 1;
			}

			auto const mode = option == "slice4" ? PicoSdCrcMode::Slicing4
			    : option == "table"              ? PicoSdCrcMode::Table
			                                     : PicoSdCrcMode::Bitwise;
			bool const ok = picoSdSetCrcMode(mode);

			ctx.writeLine(ok ? "sd-diag: CRC16 mode updated" : "sd-diag: CRC16 mode change rejected (not ready/busy)");
			return ok ? 0 : 1;
		}

		if (action == "dma") {
			if (inv.argc != 3 && !(inv.argc == 4 && std::string_view(inv.argv[2]) == "wait")) {
				ctx.writeLine("usage: sd-diag dma [status | irq-info | probe | on | off | direct | buffered | wait poll|irq|event]");
				return 1;
			}

			auto const option = std::string_view(inv.argv[2]);

			if (option == "irq-info") {
				// Read-only; preserved miss snapshots survive PL330 INTCLR and GIC teardown.
				auto const pl330 = picoSdDmaIrqRegisters();
				if (pl330.available) {
					ctx.write("PL330 INTEN/INTSTATUS/FSRD/FSRC: ");
					hex(ctx, pl330.enabledEvents);
					ctx.write(" / ");
					hex(ctx, pl330.pendingEvents);
					ctx.write(" / ");
					hex(ctx, pl330.managerFault);
					ctx.write(" / ");
					hex(ctx, pl330.channelFaults);
					ctx.writeLine("");
					ctx.write("PL330 ES/GRF_SOC_CON38/39: ");
					hex(ctx, pl330.eventStatus);
					ctx.write(" / ");
					hex(ctx, pl330.grfSocCon38);
					ctx.write(" / ");
					hex(ctx, pl330.grfSocCon39);
					ctx.writeLine("");
				} else {
					ctx.writeLine("PL330 registers unavailable; run sd-diag dma probe");
				}

				auto const missed = picoSdDmaMissedIrq();
				if (missed.valid) {
					ctx.write("MISSED IRQ148 PL330 INTEN/ES/INTSTATUS: ");
					hex(ctx, missed.pl330Inten);
					ctx.write(" / ");
					hex(ctx, missed.pl330EventStatus);
					ctx.write(" / ");
					hex(ctx, missed.pl330Intstatus);
					ctx.writeLine("");
					ctx.write("MISSED IRQ148 GRF_SOC_CON38/39: ");
					hex(ctx, missed.grfSocCon38);
					ctx.write(" / ");
					hex(ctx, missed.grfSocCon39);
					ctx.writeLine("");

					if (missed.gicValid) {
						ctx.write("MISSED IRQ148 GIC enabled/pending/active: ");
						lexxos::apps::writeUnsigned(ctx, missed.gicEnabled);
						ctx.write("/");
						lexxos::apps::writeUnsigned(ctx, missed.gicPending);
						ctx.write("/");
						lexxos::apps::writeUnsigned(ctx, missed.gicActive);
						ctx.write(" target/priority/PMR/HPPIR: ");
						hex(ctx, missed.gicTarget);
						ctx.write(" / ");
						hex(ctx, missed.gicPriority);
						ctx.write(" / ");
						hex(ctx, missed.gicPmr);
						ctx.write(" / ");
						hex(ctx, missed.gicHppir);
						ctx.writeLine("");
					}
				}

				if (missed.gicBanksValid) {
					ctx.writeLine("MISSED GIC bank: ENABLED / PENDING / ACTIVE (CPU0 view)");
					for (std::uint32_t bank = 0; bank < missed.gicBankCount; ++bank) {
						ctx.write("  ");
						lexxos::apps::writeUnsigned(ctx, bank);
						ctx.write(" [");
						lexxos::apps::writeUnsigned(ctx, bank * 32U);
						ctx.write(".. ");
						lexxos::apps::writeUnsigned(ctx, bank * 32U + 31U);
						ctx.write("]: ");
						hex(ctx, missed.gicEnabledBanks[bank]);
						ctx.write(" / ");
						hex(ctx, missed.gicPendingBanks[bank]);
						ctx.write(" / ");
						hex(ctx, missed.gicActiveBanks[bank]);
						ctx.writeLine("");
					}
				}
#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)

				constexpr std::uint32_t ids[]{27U, 148U, 149U};
				for (std::uint32_t const id : ids) {
					GicIrqSnapshot snapshot{};
					if (!rk3506_gic_irq_snapshot(id, &snapshot)) {
						ctx.writeLine("GIC snapshot unavailable");
						continue;
					}

					ctx.write("GIC IRQ ");
					lexxos::apps::writeUnsigned(ctx, id);
					ctx.write(" world=");
					ctx.write(snapshot.security.executionWorld == lexxos::kernel::IrqExecutionWorld::NonSecure     ? "non-secure"
					        : snapshot.security.executionWorld == lexxos::kernel::IrqExecutionWorld::Secure        ? "secure"
					        : snapshot.security.executionWorld == lexxos::kernel::IrqExecutionWorld::NotApplicable ? "n/a"
					                                                                                               : "unknown");
					ctx.write(" group=");
					ctx.write(snapshot.security.group == lexxos::kernel::IrqSecurityGroup::Group0 ? "0"
					        : snapshot.security.group == lexxos::kernel::IrqSecurityGroup::Group1 ? "1"
					                                                                              : "unknown");
					ctx.write(" group-readable=");
					ctx.write(snapshot.security.readable ? "yes" : "no");
					ctx.write(" group-configurable=");
					ctx.write(snapshot.security.configurable ? "yes" : "no");
					ctx.write(" enabled/pending/active: ");
					lexxos::apps::writeUnsigned(ctx, snapshot.enabled);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, snapshot.pending);
					ctx.write("/");
					lexxos::apps::writeUnsigned(ctx, snapshot.active);
					ctx.write(" edge/priority/target: ");
					lexxos::apps::writeUnsigned(ctx, snapshot.edgeTriggered);
					ctx.write("/");
					hex(ctx, snapshot.priority);
					ctx.write("/");

					if (snapshot.targetReadable) {
						hex(ctx, snapshot.target);
					} else {
						ctx.write("banked");
					}

					ctx.writeLine("");
					ctx.write("GICD_CTLR/GICC_CTLR/PMR/HPPIR: ");
					hex(ctx, snapshot.distributorControl);
					ctx.write(" / ");
					hex(ctx, snapshot.cpuControl);
					ctx.write(" / ");
					hex(ctx, snapshot.cpuPriorityMask);
					ctx.write(" / ");
					hex(ctx, snapshot.cpuHighestPending);
					ctx.writeLine("");
				}

				ctx.write("DMAC0 second output IRQ149: GIC deliveries since boot (read-only, NOT enabled by LexxOS): ");
				lexxos::apps::writeUnsigned(ctx, rk3506_gic_irq_delivery_count(149U));
				ctx.writeLine("");
#else
				ctx.writeLine("GIC snapshots require LEXXOS_RK3506_MANAGED_TIMER_IRQ=ON");
#endif
				return 0;
			}

			if (option == "wait") {
				if (inv.argc != 4) {
					ctx.writeLine("usage: sd-diag dma wait [poll | irq | event]");
					return 1;
				}

				auto const arg = std::string_view(inv.argv[3]);
				if (arg != "poll" && arg != "irq" && arg != "event") {
					ctx.writeLine("usage: sd-diag dma wait [poll | irq | event]");
					return 1;
				}

				if (!picoSdDiagnosticStatus().spi1Active || !picoSdDiagnosticStatus().ready || picoSdDiagnosticStatus().spi1Fault || picoSdDiagnosticStatus().csAsserted) {
					ctx.writeLine("sd-diag: SPI1 not probed/idle");
					return 1;
				}

				bool const ok = arg == "event" ? picoSdDmaSetWaitEvent(true) : picoSdDmaSetWaitIrq(arg == "irq");
				if (!ok) {
					ctx.writeLine("sd-diag: DMA wait change refused (check DMA probe, Group1 IRQ or event-stream ownership)");
					return 1;
				}

				ctx.writeLine(arg == "event" ? "sd-diag: timer event-stream/WFE wait EXPERIMENTAL (no DMA IRQ)"
				        : arg == "irq"       ? "sd-diag: IRQ/WFE wait EXPERIMENTAL"
				                             : "sd-diag: polling wait selected");
				return 0;
			}

			if (option == "probe") {
				if (!picoSdDiagnosticStatus().spi1Active || !picoSdDiagnosticStatus().ready || picoSdDiagnosticStatus().spi1Fault || picoSdDiagnosticStatus().csAsserted) {
					ctx.writeLine("sd-diag: SPI1 must be probed and idle first");
					return 1;
				}

				bool const ok = picoSdDmaProbe();
				ctx.writeLine(ok ? "sd-diag: DMAC0 PL330 probe OK (no transfer)" : "sd-diag: DMAC0 probe refused");

				if (!ok) {
					ctx.writeLine(picoSpi1DmaInfo().reason);
				}

				return ok ? 0 : 1;
			}

			if (option == "direct" || option == "buffered") {
				if (!picoSdDiagnosticStatus().spi1Active || !picoSdDiagnosticStatus().ready || picoSdDiagnosticStatus().spi1Fault || picoSdDiagnosticStatus().csAsserted || !picoSdDmaSetDirect(option == "direct")) {
					ctx.writeLine("sd-diag: DMA target change refused (probe SPI1/DMA first)");
					return 1;
				}

				ctx.writeLine(option == "direct"
				        ? "sd-diag: direct DMA EXPERIMENTAL (caller buffer may change on failure)"
				        : "sd-diag: buffered DMA selected (safe fallback)");
				return 0;
			}

			if (option == "on" || option == "off") {
				if (!picoSdDiagnosticStatus().spi1Active || !picoSdDiagnosticStatus().ready || picoSdDiagnosticStatus().spi1Fault || picoSdDiagnosticStatus().csAsserted || !picoSdDmaSetEnabled(option == "on")) {
					ctx.writeLine("sd-diag: DMA mode change refused (probe DMA first)");
					return 1;
				}

				ctx.writeLine(option == "on" ? "sd-diag: DMA enabled EXPERIMENTAL" : "sd-diag: PIO selected");
				return 0;
			}

			if (option != "status") {
				ctx.writeLine("usage: sd-diag dma [status | irq-info | probe | on | off | direct | buffered | wait poll|irq|event]");
				return 1;
			}

			auto const info = picoSpi1DmaInfo();
			auto const stats = picoSdDmaStatistics();
			ctx.write("DMA mode: ");
			ctx.writeLine(picoSdDmaEnabled() ? "on EXPERIMENTAL" : "off (PIO)");
			ctx.write("DMAC0: ");
			ctx.writeLine(info.available ? "probed/ready" : info.reason);
			ctx.writeLine("DMA requests: SPI1 TX=2 RX=3, channels TX=1 RX=0");
			ctx.write("DMA completion wait: ");
			ctx.writeLine(picoSdDmaWaitIrq() ? "IRQ/WFE EXPERIMENTAL"
			        : picoSdDmaWaitEvent()   ? "CNTVCT event-stream/WFE EXPERIMENTAL"
			                                 : "poll (safe default)");
			ctx.write("DMA IRQ deliveries/WFE sleeps/poll checks: ");
			lexxos::apps::writeUnsigned(ctx, stats.irqInterrupts);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.irqSleeps);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.waitPolls);
			ctx.writeLine("");
			ctx.write("DMA lost-IRQ -> polling fallbacks: ");
			lexxos::apps::writeUnsigned(ctx, stats.irqPollFallbacks);
			ctx.writeLine("");
			ctx.write("DMA IRQ148 RX/TX/both/empty ISR events: ");
			lexxos::apps::writeUnsigned(ctx, stats.irqRxEvents);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.irqTxEvents);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.irqCombinedEvents);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.irqEmptyInterrupts);
			ctx.writeLine("");
			ctx.write("DMA timer-event WFE sleeps: ");
			lexxos::apps::writeUnsigned(ctx, stats.eventSleeps);
			ctx.writeLine("");
			ctx.write("DMA target: ");
			ctx.writeLine(picoSdDmaDirect() ? "DIRECT experimental (buffered fallback)" : "buffered (safe default)");
			ctx.write("DMA direct/buffered/fallback sectors: ");
			lexxos::apps::writeUnsigned(ctx, stats.directReads);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.bufferedReads);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.directFallbacks);
			ctx.writeLine("");
			ctx.write("DMA reads/bytes/errors/timeouts: ");
			lexxos::apps::writeUnsigned(ctx, stats.reads);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.bytes);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.errors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.timeouts);
			ctx.writeLine("");
			ctx.write("DMA wait checks/spin polls/IRQ-WFE/event-WFE: ");
			lexxos::apps::writeUnsigned(ctx, stats.waitChecks);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.waitPolls);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.irqSleeps);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.eventSleeps);
			ctx.writeLine("");

			auto const hz = rk3506_timer_counter_frequency();
			auto const nonWfe = detail::dmaNonWfeWindowCycles(
			    stats.waitCycles, stats.irqWfeCycles, stats.eventWfeCycles);
			ctx.write("DMA wait wall-time us total/IRQ-WFE/event-WFE/non-WFE: ");

			lexxos::apps::writeUnsigned64(ctx, hz ? stats.waitCycles * 1000000ULL / hz : 0ULL);
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, hz ? stats.irqWfeCycles * 1000000ULL / hz : 0ULL);
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, hz ? stats.eventWfeCycles * 1000000ULL / hz : 0ULL);
			ctx.write("/");
			lexxos::apps::writeUnsigned64(ctx, hz ? nonWfe * 1000000ULL / hz : 0ULL);
			ctx.writeLine(" (WFE includes IRQ time, NOT CPU utilisation)");

			if (stats.reads != 0U && hz != 0U) {
				auto printUs = [&](std::uint64_t cycles) {
					lexxos::apps::writeUnsigned64(ctx, cycles * 1000000ULL / hz);
				};

				ctx.write("DMA phase us setup/launch/wait/finish/copy/total: ");
				printUs(stats.setupCycles);
				ctx.write("/");
				printUs(stats.launchCycles);
				ctx.write("/");
				printUs(stats.waitCycles);
				ctx.write("/");
				printUs(stats.finishCycles);
				ctx.write("/");
				printUs(stats.copyCycles);
				ctx.write("/");
				printUs(stats.payloadCycles);
				ctx.writeLine(" (cumulative; wait includes failed transfers)");
			}

			auto const failure = picoSdDmaFailure();
			if (failure.valid) {
				ctx.write("DMA first-failure stage: ");
				ctx.writeLine(failure.stage);
				auto pair = [&ctx](char const* label, std::uint32_t a, std::uint32_t b) {
					ctx.write(label);
					hex(ctx, a);
					ctx.write(" / ");
					hex(ctx, b);
					ctx.writeLine("");
				};
				pair("DMA DS/INTSTATUS: ", failure.manager, failure.events);
				pair("DMA RX-CS/TX-CS: ", failure.rxChannel, failure.txChannel);
				pair("DMA FSM/FSC: ", failure.managerFault, failure.channelFaults);
				pair("DMA FTC-RX/FTC-TX: ", failure.rxFaultType, failure.txFaultType);
				pair("DMA CPC-RX/CPC-TX: ", failure.rxPc, failure.txPc);
				pair("SPI SR/RISR: ", failure.spiStatus, failure.spiIrq);
				pair("SPI TXFLR/RXFLR: ", failure.spiTxLevel, failure.spiRxLevel);
				pair("SPI DMACR/TX-threshold: ", failure.spiDmaControl, failure.spiTxThreshold);
				pair("SPI RX-threshold/GRF42: ", failure.spiRxThreshold, failure.grfSocCon42);
				pair("SPI SSIENR/SER: ", failure.spiEnabled, failure.spiSlaveEnabled);
				pair("DMA code RX/TX base: ", failure.rxCodeAddress, failure.txCodeAddress);
			}

			return 0;
		}

		if (action == "profile") {
			if (inv.argc == 3 && std::string_view(inv.argv[2]) == "reset") {
				bool const ok = picoSdResetReadProfileStatistics();
				ctx.writeLine(ok ? "sd-diag: profile reset" : "sd-diag: profile busy");
				return ok ? 0 : 1;
			}

			if (inv.argc != 2 && !(inv.argc == 3 && std::string_view(inv.argv[2]) == "status")) {
				ctx.writeLine("usage: sd-diag profile [status | reset]");
				return 1;
			}

			auto const stats = picoSdReadProfileStatistics();
			ctx.write("SD profile CMD17/CMD18/CMD12: ");
			lexxos::apps::writeUnsigned(ctx, stats.cmd17);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.cmd18);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.cmd12);
			ctx.writeLine("");
			ctx.write("SD profile sectors/token-polls: ");
			lexxos::apps::writeUnsigned(ctx, stats.goodSectors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.tokenPolls);
			ctx.writeLine("");
			ctx.write("SD profile token/CRC/read-errors: ");
			lexxos::apps::writeUnsigned(ctx, stats.tokenErrors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.crcErrors);
			ctx.write("/");
			lexxos::apps::writeUnsigned(ctx, stats.readErrors);
			ctx.writeLine("");
			auto const hz = rk3506_timer_counter_frequency();
			if (hz != 0U) {
				ctx.write("SD profile us cmd/token/CRCwire/CRCmath/CMD12: ");
				auto printCycles = [&](std::uint64_t cycles) {
					lexxos::apps::writeUnsigned64(ctx, cycles * 1000000ULL / hz);
				};
				printCycles(stats.commandCycles);
				ctx.write("/");
				printCycles(stats.tokenCycles);
				ctx.write("/");
				printCycles(stats.crcWireCycles);
				ctx.write("/");
				printCycles(stats.crcComputeCycles);
				ctx.write("/");
				printCycles(stats.stopCycles);
				ctx.writeLine("");
			}

			return 0;
		}

		if (action == "clock") {
			if (inv.argc != 3) {
				ctx.writeLine("usage: sd-diag clock [status | 6 | 12]");
				return 1;
			}

			auto const setting = std::string_view(inv.argv[2]);
			if (setting == "status") {
				if (!picoSdDiagnosticStatus().spi1Active || picoSdDiagnosticStatus().spi1Fault || !picoSdDiagnosticStatus().ready) {
					ctx.writeLine("sd-diag: hardware SPI1 is not ready; run sd-diag mode spi1, then probe");
					return 1;
				}

				ctx.write("sd-diag: SPI1 clock: ");
				std::uint32_t const mhz = picoSpi1ClockMHz();
				ctx.writeLine(mhz == 6U ? "6 MHz" : mhz == 12U ? "12 MHz"
				                                               : "unknown");
				return 0;
			}

			if ((setting != "6" && setting != "12") || !picoSdDiagnosticStatus().spi1Active || picoSdDiagnosticStatus().spi1Fault || !picoSdDiagnosticStatus().ready) {
				ctx.writeLine("sd-diag: usage: sd-diag clock [status | 6 | 12] (probe SPI1 first)");
				return 1;
			}

			// CS is GPIO-controlled: neither a pending SD command nor a
			// partially received data block may cross the frequency change.
			if (picoSdDiagnosticStatus().csAsserted || !picoSpi1SetClockMHz(setting == "12" ? 12U : 6U)) {
				ctx.writeLine("sd-diag: SPI1 busy, clock unchanged");
				return 1;
			}

			ctx.write("sd-diag: SPI1 clock set to ");
			ctx.writeLine(setting == "12" ? "12 MHz" : "6 MHz");
			return 0;
		}

		if (action == "fifo") {
			if (inv.argc != 3 || !picoSdDiagnosticStatus().spi1Active || picoSdDiagnosticStatus().spi1Fault || !picoSdDiagnosticStatus().ready) {
				ctx.writeLine("sd-diag: usage: sd-diag fifo [safe | fast | reset] (probe SPI1 first)");
				return 1;
			}

			auto const option = std::string_view(inv.argv[2]);

			bool ok = false;
			if (option == "reset") {
				ok = picoSpi1ResetFifoStatistics();
			} else if (option == "safe" || option == "fast") {
				ok = picoSpi1SetFifoMode(option == "fast" ? PicoSpi1FifoMode::Fast : PicoSpi1FifoMode::Safe);
			} else {
				ctx.writeLine("sd-diag: usage: sd-diag fifo [safe | fast | reset]");
				return 1;
			}

			ctx.writeLine(ok ? "sd-diag: FIFO setting updated" : "sd-diag: SPI1 FIFO is busy or unavailable");
			return ok ? 0 : 1;
		}

		if (action == "mode") {
			if (inv.argc != 3 || picoSdFatMounted()) {
				ctx.writeLine("sd-diag: unmount /mnt/sd first; usage: sd-diag mode [gpio | spi1]");
				return 1;
			}

			auto const mode = std::string_view(inv.argv[2]);
			if (mode != "gpio" && mode != "spi1") {
				ctx.writeLine("usage: sd-diag mode [gpio | spi1]");
				return 1;
			}

			// Manual transport changes suppress automount until card reinsertion.
			picoSdAutoSuppressUntilRemoval();
			(void) picoSdSelectTransport(mode == "spi1");
			ctx.writeLine(picoSdDiagnosticStatus().requestSpi1 ? "sd-diag: SPI1 selected; run 'sd-diag probe'" : "sd-diag: GPIO selected; run 'sd-diag probe'");
			return 0;
		}

		if (action == "status") {
			auto const registers = picoSdRegisterStatus();
			auto const gate = registers.gpioGate;
			auto const spiGate = registers.spiGate;

			ctx.writeLine("PicoCalc microSD: separate SPI slot, RK3506 GPIO1 / SPI1");
			ctx.write("requested transport: ");
			ctx.writeLine(picoSdDiagnosticStatus().requestSpi1 ? "hardware SPI1" : "GPIO bitbang");
			ctx.write("active transport: ");
			ctx.writeLine(picoSdDiagnosticStatus().spi1Active ? "hardware SPI1" : "GPIO bitbang");
			ctx.write("SD CRC16 mode: ");
			ctx.writeLine(picoSdDiagnosticStatus().crcMode == PicoSdCrcMode::Slicing4 ? "slice4"
			        : picoSdDiagnosticStatus().crcMode == PicoSdCrcMode::Table        ? "table"
			                                                                          : "bitwise");

			if (picoSdDiagnosticStatus().spi1Active) {
				ctx.write("SPI1 version: ");
				hex(ctx, picoSpi1Version());
				ctx.write("  divider: ");
				hex(ctx, picoSpi1Divider());
				ctx.write("  clock: ");
				std::uint32_t const mhz = picoSpi1ClockMHz();
				ctx.write(mhz == 6U ? "6 MHz" : mhz == 12U ? "12 MHz"
				                                           : "unknown");
				ctx.write("  FIFO mode: ");
				ctx.writeLine(picoSpi1FifoMode() == PicoSpi1FifoMode::Fast ? "fast" : "safe");

				auto const stats = picoSpi1FifoStatistics();
				ctx.write("SPI1 FIFO reads/bytes/bursts: ");
				hex(ctx, stats.reads);
				ctx.write(" / ");
				hex(ctx, stats.bytes);
				ctx.write(" / ");
				hex(ctx, stats.txBursts);
				ctx.writeLine("");
				ctx.write("SPI1 FIFO peak pending/RX: ");
				hex(ctx, stats.peakPending);
				ctx.write(" / ");
				hex(ctx, stats.peakRxFifo);
				ctx.writeLine("");
				ctx.write("SPI1 FIFO RX polls/empty/drains: ");
				hex(ctx, stats.rxPolls);
				ctx.write(" / ");
				hex(ctx, stats.emptyRxPolls);
				ctx.write(" / ");
				hex(ctx, stats.rxDrainBursts);
				ctx.writeLine("");
				ctx.write("SPI1 FIFO IRQ/TXFLR checks: ");
				hex(ctx, stats.irqChecks);
				ctx.write(" / ");
				hex(ctx, stats.txLevelChecks);
				ctx.writeLine("");
				ctx.write("SPI1 FIFO timeout/overrun/sync: ");
				hex(ctx, stats.timeouts);
				ctx.write(" / ");
				hex(ctx, stats.overflows);
				ctx.write(" / ");
				hex(ctx, stats.syncErrors);
				ctx.writeLine("");
				ctx.write("SPI1 FIFO last SR/RISR: ");
				hex(ctx, stats.lastStatus);
				ctx.write(" / ");
				hex(ctx, stats.lastRawIrq);
				ctx.writeLine("");
			}

			ctx.write("GPIO1 clock gate: ");
			hex(ctx, gate);
			ctx.writeLine("");
			ctx.write("SPI1 clock gate: ");
			hex(ctx, spiGate);
			ctx.writeLine("");
			ctx.write("GPIO1 IOC C: ");
			hex(ctx, registers.iocC);
			ctx.writeLine("");
			ctx.write("GPIO1 IOC D: ");
			hex(ctx, registers.iocD);
			ctx.writeLine("");
			ctx.write("GPIO configured: ");
			ctx.writeLine(picoSdDiagnosticStatus().configured ? "yes" : "no");
			if (registers.detectReadable) {
				ctx.write("card detect GPIO1_B2: ");
				ctx.writeLine(registers.detectHigh ? "high (absent)" : "low (present)");
			} else {
				ctx.writeLine("card detect: GPIO1 clock is gated");
			}

			ctx.write("probe-ready: ");
			ctx.writeLine(picoSdDiagnosticStatus().ready ? "yes" : "no");
			ctx.write("FAT mounted: ");
			ctx.writeLine(picoSdFatMounted() ? "yes (/mnt/sd ro)" : "no");
			ctx.write("last step: ");
			ctx.writeLine(picoSdDiagnosticStatus().step);
			ctx.write("last R1: ");
			hexByte(ctx, picoSdDiagnosticStatus().response);
			ctx.writeLine("");
			if (picoSdDiagnosticStatus().ready) {
				ctx.write("OCR: ");
				hex(ctx, picoSdDiagnosticStatus().ocr);
				ctx.writeLine("");
			}

			return 0;
		}

		if (action == "probe") {
			if (picoSdFatMounted()) {
				ctx.writeLine("sd-diag: unmount /mnt/sd before probe");
				return 1;
			}

			ctx.writeLine(picoSdDiagnosticStatus().requestSpi1 ? "PicoCalc SPI-SD: hardware SPI1, read-only" : "PicoCalc SPI-SD: GPIO bitbang, read-only");

			if (!picoSdProbeManual()) {
				ctx.write("sd-diag: probe failed at ");
				ctx.write(picoSdDiagnosticStatus().step);
				ctx.write(" R1=");
				hexByte(ctx, picoSdDiagnosticStatus().response);
				ctx.writeLine("");
				return 1;
			}

			ctx.write("sd-diag: ready; OCR=");
			hex(ctx, picoSdDiagnosticStatus().ocr);

			ctx.writeLine(picoSdDiagnosticStatus().highCapacity ? " SDHC/SDXC" : " SDSC");
			ctx.write("sd-diag: sectors (CSD): ");
			char digits[21]{};
			std::size_t pos = sizeof(digits) - 1U;
			auto count = picoSdDiagnosticStatus().sectorCount;

			do {
				digits[--pos] = char('0' + count % 10U);
				count /= 10U;
			} while (count);

			ctx.writeLine(digits + pos);
			return 0;
		}

		if (action == "read") {
			if (inv.argc != 3 || std::string_view(inv.argv[2]) != "0") {
				ctx.writeLine("usage: sd-diag read 0  (first stage allows LBA 0 only)");
				return 1;
			}

			struct alignas(4) Guarded {
				std::uint32_t before;
				std::uint8_t bytes[kSectorSize];
				std::uint32_t after;
			} sector{0xA55A1EAFU, {}, 0x5AA5E1F0U};

			if (!picoSdReadSector(0, sector.bytes)) {
				ctx.write("sd-diag: read failed at ");
				ctx.write(picoSdDiagnosticStatus().step);
				ctx.write(" R1=");
				hexByte(ctx, picoSdDiagnosticStatus().response);
				ctx.writeLine("");
				return 1;
			}

			if (sector.before != 0xA55A1EAFU || sector.after != 0x5AA5E1F0U) {
				ctx.writeLine("sd-diag: buffer guard corrupted");
				return 1;
			}

			ctx.writeLine("sd-diag: LBA 0 first 64 bytes:");
			for (std::size_t i = 0; i < 64; ++i) {
				hexByte(ctx, sector.bytes[i]);
				ctx.write(i % 16U == 15U ? "\r\n" : " ");
			}

			ctx.write("MBR signature: ");
			ctx.writeLine(sector.bytes[510] == 0x55U && sector.bytes[511] == 0xAAU ? "55aa" : "missing");
			return 0;
		}

		ctx.writeLine("usage: sd-diag [status | profile ... | crc ... | dma ... | clock ... | fifo ... | probe | read 0]");

		return 1;
	}

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	struct GicTableRow {
		char chars[112]{};
		std::uint32_t size{};

		void append(char ch) {
			if (size + 1U < sizeof(chars)) {
				chars[size++] = ch;
				chars[size] = '\0';
			}
		}

		void append(char const* text) {
			while (*text != '\0') {
				append(*text++);
			}
		}

		void number(std::uint32_t n) {
			char digits[10]{};
			std::uint32_t count{};
			do {
				digits[count++] = static_cast<char>('0' + (n % 10U));
				n /= 10U;
			} while (n != 0U);

			while (count != 0U) {
				append(digits[--count]);
			}
		}

		void padded(std::uint32_t n, std::uint32_t width) {
			std::uint32_t digits = 1U;
			for (auto v = n; v >= 10U; v /= 10U) {
				++digits;
			}

			while (digits++ < width) {
				append(' ');
			}

			number(n);
		}

		void hex(std::uint32_t n) {
			constexpr char digits[] = "0123456789abcdef";
			append('0');
			append('x');
			append(digits[(n >> 4U) & 15U]);
			append(digits[n & 15U]);
		}
	};

	bool parseGicId(char const* value, std::uint32_t& result) {
		if (value == nullptr || *value == '\0') {
			return false;
		}

		std::uint32_t n{};
		for (auto p = value; *p != '\0'; ++p) {
			if (*p < '0' || *p > '9') {
				return false;
			}

			if (n > (maxGicIntids - 1U) / 10U) {
				return false;
			}

			n = n * 10U + static_cast<std::uint32_t>(*p - '0');
			if (n >= maxGicIntids) {
				return false;
			}
		}

		result = n;
		return true;
	}

	int runGicTable(lexxos::os::AppContext& ctx,
	    lexxos::program::ProgramInvocation const& inv,
	    lexxos::program::ParsedArguments const&) {
		// Read-only: never fetch GICC_IAR, modify GICD, or take the SPI1 queue.
		GicIrqBankSnapshot banks{};
		if (!rk3506_gic_irq_bank_snapshot(&banks)) {
			ctx.writeLine("irq-list: GIC unavailable");
			return 1;
		}

		auto const count = banks.lineCount < maxGicIntids ? banks.lineCount : maxGicIntids;
		enum class Filter {
			Interesting,
			All,
			Enabled,
			Pending,
			Active,
			Handled,
			Range
		};

		Filter filter = Filter::Interesting;
		std::uint32_t first{}, last = count != 0U ? count - 1U : 0U;
		if (inv.argc == 2) {
			std::string_view const arg(inv.argv[1]);
			if (arg == "all") {
				filter = Filter::All;
			} else if (arg == "enabled") {
				filter = Filter::Enabled;
			} else if (arg == "pending") {
				filter = Filter::Pending;
			} else if (arg == "active") {
				filter = Filter::Active;
			} else if (arg == "handled") {
				filter = Filter::Handled;
			} else if (parseGicId(inv.argv[1], first)) {
				filter = Filter::Range;
				last = first;
			} else {
				ctx.writeLine("usage: irq-list [all|enabled|pending|active|handled|id [last-id]]");
				return 1;
			}
		} else if (inv.argc == 3) {
			filter = Filter::Range;
			if (!parseGicId(inv.argv[1], first) || !parseGicId(inv.argv[2], last) || first > last) {
				ctx.writeLine("usage: irq-list [all|enabled|pending|active|handled|id [last-id]]");
				return 1;
			}
		} else if (inv.argc != 1) {
			ctx.writeLine("usage: irq-list [all|enabled|pending|active|handled|id [last-id]]");
			return 1;
		}

		ctx.write("GIC INTIDs: ");
		lexxos::apps::writeUnsigned(ctx, count);
		ctx.writeLine(" (CPU0 Non-secure view; zeros can be RAZ, groups unknown)");
		ctx.writeLine(" ID TYPE SPI EN PD AC TR PRI CPU       HITS  SOURCE");
		ctx.writeLine("----------------------------------------------------------");
		std::uint32_t shown{};

		for (std::uint32_t id = 0; id < count; ++id) {
			std::uint32_t const bit = 1U << (id % 32U);
			std::uint32_t const bank = id / 32U;
			bool const en = (banks.enabled[bank] & bit) != 0U;
			bool const pd = (banks.pending[bank] & bit) != 0U;
			bool const ac = (banks.active[bank] & bit) != 0U;
			std::uint32_t const hits = rk3506_gic_irq_delivery_count(id);

			bool const selected = filter == Filter::All || (filter == Filter::Interesting && (en || pd || ac || hits != 0U || id == 27U || id == 148U || id == 149U)) || (filter == Filter::Enabled && en) || (filter == Filter::Pending && pd) || (filter == Filter::Active && ac) || (filter == Filter::Handled && hits != 0U) || (filter == Filter::Range && id >= first && id <= last);
			if (!selected) {
				continue;
			}

			GicIrqSnapshot snapshot{};
			if (!rk3506_gic_irq_snapshot(id, &snapshot)) {
				continue;
			}

			GicTableRow row{};
			row.padded(id, 3U);
			row.append(' ');
			row.append(gicKind(id));
			row.append(' ');
			if (id < 32U) {
				row.append("  -");
			} else {
				row.padded(id - 32U, 3U);
			}

			row.append(' ');
			row.number(en);
			row.append("  ");
			row.number(pd);
			row.append("  ");
			row.number(ac);
			row.append("  ");
			row.append(snapshot.edgeTriggered ? 'E' : 'L');
			row.append("  ");
			row.hex(snapshot.priority);
			row.append(' ');
			if (snapshot.targetReadable) {
				row.hex(snapshot.target);
			} else {
				row.append("banked");
			}

			row.append(' ');
			row.padded(hits, 10U);
			row.append("  ");
			row.append(gicKnownName(id));
			ctx.writeLine(row.chars);
			++shown;
		}

		ctx.write("Rows: ");
		lexxos::apps::writeUnsigned(ctx, shown);
		ctx.writeLine(". HITS count only IRQs ACKed by LexxOS CPU0 since boot.");
		return 0;
	}
#endif

} // namespace

void registerPicoSdProgram(lexxos::program::ProgramManager& programs) {
	programs.registerProgram({
	    .name = "sd-diag",
	    .help = "PicoCalc microSD GPIO/SPI1 diagnostics (maintenance builds)",
	    .description = "GPIO or SPI1 on PicoCalc socket; never touches boot SDMMC",
	    .usage = "sd-diag [status | probe | read 0 | dma ...]",
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                },
	    .allowExtraArguments = true,
	    .enabled = true,
	});

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	programs.registerProgram({
	    .name = "irq-list",
	    .help = "Read-only RK3506 GICv2 interrupt table and CPU0 delivery counts",
	    .description = "All implemented GIC INTIDs, visible states and delivered IRQ counts",
	    .usage = "irq-list [all|enabled|pending|active|handled|id [last-id]]",
	    .handler = runGicTable,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                },
	    .allowExtraArguments = true,
	    .enabled = true,
	});
#endif
}

} // namespace lexxos::platform::rk3506
