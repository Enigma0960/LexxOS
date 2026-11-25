#include <string_view>

#include <lexxos/composition/products/lyra_ram_reload_session.h>
#include <lexxos/composition/products/lyra_spi_queues.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_manager.h>

extern "C" void rk3506_console_write(char const* text);
extern "C" bool rk3506_debug_console_select(unsigned route);
extern "C" unsigned rk3506_debug_console_mode();
extern "C" bool rk3506_debug_switch_swd();
extern "C" bool rk3506_debug_swd_enabled();
extern "C" bool rk3506_debug_prepare_cpu_for_ram_reload();
extern "C" [[noreturn]] void rk3506_debug_park_for_ram_reload();

namespace lexxos::platform::rk3506 {

namespace {

	int run(lexxos::os::AppContext& ctx, lexxos::program::ProgramInvocation const& inv,
	    lexxos::program::ParsedArguments const&) {
		auto const command = inv.argc > 1 ? std::string_view(inv.argv[1]) : "status";
		if (command == "status" && (inv.argc == 1 || inv.argc == 2)) {
			ctx.write("console: ");
			switch (rk3506_debug_console_mode()) {
				case 0 : ctx.writeLine("uart0 (1500000)"); break;
				case 1 : ctx.writeLine("uart1 (1500000)"); break;
				case 2 : ctx.writeLine("mirror (UART1 RX priority)"); break;
				default: ctx.writeLine("unknown"); break;
			}

			ctx.writeLine(rk3506_debug_swd_enabled() ? "Cortex-A7 SWD: on" : "Cortex-A7 SWD: off");
			return 0;
		}

		if (command == "uart" && inv.argc == 3) {
			auto const arg = std::string_view(inv.argv[2]);
			unsigned route = 3;
			if (arg == "0") {
				route = 0;
			} else if (arg == "1") {
				route = 1;
			} else if (arg == "mirror") {
				route = 2;
			}

			if (route > 2) {
				ctx.writeLine("usage: debug uart 0|1|mirror");
				return 1;
			}

			if (rk3506_debug_swd_enabled() && route != 1) {
				ctx.writeLine("SWD owns UART0 pins: UART1 is mandatory until power-cycle");
				return 1;
			}

			ctx.writeLine("Switching console: next response will use the selected UART");
			if (!rk3506_debug_console_select(route)) {
				ctx.writeLine("UART switch failed; previous console route preserved");
				return 1;
			}

			ctx.writeLine("Console route changed");
			return 0;
		}

		if (command == "swd" && inv.argc == 3 && std::string_view(inv.argv[2]) == "on") {
#if defined(LEXXOS_RK3506_SWD_DEBUG)
			if (rk3506_debug_swd_enabled()) {
				ctx.writeLine("SWD already enabled");
				return 0;
			}

			ctx.writeLine("Switching to UART1; UART0 pads will become SWCLK/SWDIO");
			if (!rk3506_debug_switch_swd()) {
				ctx.writeLine("UART1 initialization failed; SWD remains off");
				return 1;
			}

			ctx.writeLine("Cortex-A7 SWD enabled. Use Raspberry Pi Debug Probe; do not reset SoC.");
			return 0;
#else
			ctx.writeLine("SWD handoff is disabled at build time (LEXXOS_RK3506_SWD_DEBUG=OFF)");
			return 1;
#endif
		}

		if (command == "ram" && inv.argc == 3 && std::string_view(inv.argv[2]) == "prepare") {
#if !defined(LEXXOS_RK3506_SWD_DEBUG)
			ctx.writeLine("RAM reload disabled: build with LEXXOS_RK3506_SWD_DEBUG=ON");
			return 1;
#else
			if (!rk3506_debug_swd_enabled()) {
				ctx.writeLine("RAM reload requires SWD and UART1: run 'debug swd on' first");
				return 1;
			}

			lexxos::composition::products::LyraRamReloadSession session{
			    lexxos::composition::products::lyraSpi0Queue(), lexxos::composition::products::lyraSpi1Queue()};
			if (!session.prepare()) {
				rk3506_console_write("RAM reload refused: SPI bus or PL330 DMA unavailable\n");
				return 1;
			}

			if (!rk3506_debug_prepare_cpu_for_ram_reload()) {
				rk3506_console_write("RAM reload refused: MMU or D-cache enabled\n");
				return 1;
			}

			// После захвата SPI выводим только в UART, обходя LCD console mirror.
			rk3506_console_write("RAM reload: CPU0 parking in SVC with IRQs disabled\n");
			rk3506_console_write("Use GDB 'lexxos_ram_reload'; no further shell input until cold boot or RAM start\n");
			rk3506_debug_park_for_ram_reload();
#endif
		}

		ctx.writeLine("usage: debug [status | uart 0|1|mirror | swd on | ram prepare]");
		ctx.writeLine("SWD -> UART0 is intentionally one-way until a cold boot.");
		return 1;
	}

} // namespace

void registerDebugProgram(lexxos::program::ProgramManager& programs) {
	programs.registerProgram({
	    .name = "debug",
	    .help = "Switch RK3506 console UART and Cortex-A7 SWD at runtime",
	    .description = "Safe SD boot on UART0; optional UART1 and one-way SWD handoff",
	    .usage = "debug [status | uart 0|1|mirror | swd on | ram prepare]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .enabled = true,
	});
}

} // namespace lexxos::platform::rk3506
