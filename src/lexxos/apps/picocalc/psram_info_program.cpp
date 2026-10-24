#include <lexxos/apps/picocalc/psram_info_program.h>

#include <array>
#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform.h>

namespace lexxos::apps::picocalc_programs {

lexxos::program::ProgramDescriptor PsramInfoProgram::descriptor() {
	return {
	    .name = "psram-info",
	    .help = "read external PSRAM JEDEC ID",
	    .description = "Reads command 0x9F from the PSRAM SPI wiring declared by the active board profile.",
	    .usage = "psram-info",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PsramInfoProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const& board = activeBoard(context);
	context.write("psram: cs=");
	writePinValue(context, board.psram.spi.cs);
	context.write(" sck=");
	writePinValue(context, board.psram.spi.sck);
	context.write(" mosi=");
	writePinValue(context, board.psram.spi.mosi);
	context.write(" miso=");
	writePinValue(context, board.psram.spi.miso);
	context.write(" sio2=");
	writePinValue(context, board.psram.sio2);
	context.write(" sio3=");
	writePinValue(context, board.psram.sio3);
	context.write(" baud=");
	writeUnsigned(context, board.psram.spi.baudRate);
	context.write("Hz");
	context.writeLine("");

	if (!board.psram.spi.isValid()) {
		context.writeLine("psram-info: PSRAM SPI wiring is not configured");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	lexxos::platform::GameBoyRomPsramRuntimeInfo runtime{};
	if (lexxos::platform::gameBoyRomPsramRuntimeInfo(runtime)) {
		context.write("psram-runtime: backend=");
		context.write(runtime.backend == nullptr ? "none" : runtime.backend);
		context.write(" effective-baud=");
		writeUnsigned(context, runtime.effectiveBaudRate);
		context.write("Hz pio=");
		context.write(runtime.pioConfigured ? "yes" : "no");
		context.write(" pio-index=");
		if (runtime.pioIndex >= 0) {
			writeUnsigned(context, static_cast<std::uint32_t>(runtime.pioIndex));
		} else {
			context.write("none");
		}
		context.write(" sm=");
		if (runtime.stateMachine >= 0) {
			writeUnsigned(context, static_cast<std::uint32_t>(runtime.stateMachine));
		} else {
			context.write("none");
		}
		context.write(" transfers=");
		writeUnsigned(context, runtime.transferCount);
		context.write(" pio-transfers=");
		writeUnsigned(context, runtime.pioTransferCount);
		context.write(" software-transfers=");
		writeUnsigned(context, runtime.softwareTransferCount);
		context.write(" pio-low-baud=");
		writeUnsigned(context, runtime.pioRejectedLowBaudCount);
		context.write(" pio-config-fail=");
		writeUnsigned(context, runtime.pioConfigureFailureCount);
		context.write(" pio-timeout=");
		writeUnsigned(context, runtime.pioTransferTimeoutCount);
		context.write(" last-pio-failure=");
		context.write(runtime.lastPioFailure == nullptr ? "none" : runtime.lastPioFailure);
		context.writeLine("");
	}

	std::array<std::uint8_t, 8> id{};
	if (!detail::readPsramJedecId(board, id)) {
		context.writeLine("psram-info: SPI transfer failed");
		return 1;
	}

	context.write("jedec-id:");
	for (auto const byte : id) {
		context.write(" ");
		writeHexByte(context, byte);
	}
	context.writeLine("");

	if (detail::idLooksIdle(id, 0x00) || detail::idLooksIdle(id, 0xFF)) {
		context.writeLine("psram-info: no PSRAM response detected");
		return 1;
	}
	return 0;
#else
	context.writeLine("psram-info: SPI probe is unavailable on this platform");
	return 1;
#endif
}

} // namespace lexxos::apps::picocalc_programs
