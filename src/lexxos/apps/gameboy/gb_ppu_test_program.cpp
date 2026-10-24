#include <lexxos/apps/gameboy/gb_ppu_test_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/devices/device_manager.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbPpuTestProgram::descriptor() {
	return {
	    .name = "gb-ppu-test",
	    .help = "render Game Boy PPU background test",
	    .description = "Seeds Game Boy VRAM with a deterministic tile background, renders it through the PPU background renderer and blits the result to the primary display.",
	    .usage = "gb-ppu-test",
	    .arguments = {},
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbPpuTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		context.writeErrorLine("gb-ppu-test: display device is not registered");
		return 1;
	}
	if (!display->isReady()) {
		context.writeErrorLine("gb-ppu-test: display device is not ready");
		return 1;
	}
	if (display->nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		context.writeErrorLine("gb-ppu-test: only rgb565 displays are supported");
		return 1;
	}

	auto displayLock = context.devices().lockDisplayDevice(display->name(), lexxos::devices::DeviceResourcePriority::Interactive, "gb-ppu-test");
	if (!displayLock) {
		context.writeErrorLine("gb-ppu-test: display device is busy");
		return 1;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& bus = workspace->bus;
	auto& framebuffer = workspace->framebuffer;
	auto& ppu = workspace->ppu;

	lexxos::gameboy::seedPpuDemoBackground(bus);
	ppu.renderBackground(bus, framebuffer);

	lexxos::gameboy::DisplayRenderInfo renderInfo{};
	if (!lexxos::gameboy::Ppu::blitFrameBuffer(*display, framebuffer, &renderInfo)) {
		context.writeErrorLine("gb-ppu-test: failed to render PPU frame");
		return 1;
	}

	context.writeLine("gb-ppu-test: rendered synthetic DMG background");
	context.write("display: ");
	writeUnsigned(context, static_cast<std::uint32_t>(display->width()));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(display->height()));
	context.write(" ");
	context.writeLine(lexxos::devices::toString(display->nativeFormat()));
	context.writeLine("framebuffer: 160x144 indexed2");
	writeDisplayRenderInfo(context, renderInfo);
	context.writeLine("ppu: ok");
	return 0;
}

} // namespace lexxos::apps::gameboy_programs
