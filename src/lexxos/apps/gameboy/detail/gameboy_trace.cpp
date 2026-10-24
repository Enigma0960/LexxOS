#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

void writeCartridgeCacheStats(lexxos::os::AppContext& context, lexxos::gameboy::Cartridge const& cartridge, char const* prefix) {
	context.write(prefix);
	context.write("cache-fills=");
	writeUnsigned(context, cartridge.cacheFillCount());
	context.write(" retries=");
	writeUnsigned(context, cartridge.cacheRetryCount());
	context.write(" failures=");
	writeUnsigned(context, cartridge.cacheFailureCount());
	context.write(" selected-bank=");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.writeLine("");
}

void writeBusIoSnapshot(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, char const* prefix) {
	context.write(prefix);
	context.write("LCDC=");
	writeCode8(context, bus.read8OrFF(0xFF40));
	context.write(" STAT=");
	writeCode8(context, bus.read8OrFF(0xFF41));
	context.write(" LY=");
	writeCode8(context, bus.read8OrFF(0xFF44));
	context.write(" LYC=");
	writeCode8(context, bus.read8OrFF(0xFF45));
	context.write(" SCX=");
	writeCode8(context, bus.read8OrFF(0xFF43));
	context.write(" SCY=");
	writeCode8(context, bus.read8OrFF(0xFF42));
	context.write(" BGP=");
	writeCode8(context, bus.read8OrFF(0xFF47));
	context.write(" IF=");
	writeCode8(context, bus.read8OrFF(0xFF0F));
	context.write(" IE=");
	writeCode8(context, bus.read8OrFF(0xFFFF));
	context.writeLine("");
}

void writeGbEmuTraceLine(
    lexxos::os::AppContext& context,
    std::uint32_t frame,
    std::uint32_t frameSteps,
    std::uint32_t frameCycles,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cartridge const& cartridge) {
	context.write("frame ");
	writeUnsigned(context, frame);
	context.write(": steps=");
	writeUnsigned(context, frameSteps);
	context.write(" cycles=");
	writeUnsigned(context, frameCycles);
	context.write(" pc=");
	writeHex16(context, cpu.pc());
	context.write(" ly=");
	writeCode8(context, bus.read8OrFF(0xFF44));
	context.write(" stat=");
	writeCode8(context, bus.read8OrFF(0xFF41));
	context.write(" if=");
	writeCode8(context, bus.read8OrFF(0xFF0F));
	context.write(" bank=");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.write(" vram=");
	writeUnsigned(context, bus.nonZeroVramBytes());
	context.writeLine("");
}

void writeGbEmuFinalSummary(
    lexxos::os::AppContext& context,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Cartridge const& cartridge) {
	context.write("gbemu: final ");
	writeCpuState(context, cpu);
	context.writeLine("");
	writeBusIoSnapshot(context, bus, "gbemu: io ");
	context.write("gbemu: selected-bank=");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.write(" vram-nonzero=");
	writeUnsigned(context, bus.nonZeroVramBytes());
	context.write("/");
	writeUnsigned(context, static_cast<std::uint32_t>(lexxos::gameboy::Bus::vramSize));
	context.write(" oam-nonzero=");
	writeUnsigned(context, bus.nonZeroOamBytes());
	context.write("/");
	writeUnsigned(context, static_cast<std::uint32_t>(lexxos::gameboy::Bus::oamSize));
	context.write(" vram-writes=");
	writeUnsigned(context, bus.vramWriteCount());
	context.write(" oam-writes=");
	writeUnsigned(context, bus.oamWriteCount());
	context.writeLine("");
	writeCartridgeCacheStats(context, cartridge, "gbemu: ");
	context.write("gbemu: xram enabled=");
	context.write(cartridge.ramEnabled() ? "1" : "0");
	context.write(" bank=");
	writeUnsigned(context, cartridge.selectedRamBank());
	context.write(" reads=");
	writeUnsigned(context, cartridge.externalRamReadCount());
	context.write(" writes=");
	writeUnsigned(context, cartridge.externalRamWriteCount());
	context.writeLine("");
	writeVramRangeStats(context, bus);
}

void writeRegisterPair(lexxos::os::AppContext& context, std::string_view name, std::uint16_t value) {
	context.write(name);
	context.write("=");
	writeHex16(context, value);
}

void writeCpuState(lexxos::os::AppContext& context, lexxos::gameboy::Cpu const& cpu) {
	writeRegisterPair(context, "PC", cpu.pc());
	context.write(" ");
	writeRegisterPair(context, "SP", cpu.sp());
	context.write(" ");
	writeRegisterPair(context, "AF", cpu.af());
	context.write(" ");
	writeRegisterPair(context, "BC", cpu.bc());
	context.write(" ");
	writeRegisterPair(context, "DE", cpu.de());
	context.write(" ");
	writeRegisterPair(context, "HL", cpu.hl());
	context.write(" IME:");
	context.write(cpu.ime() ? "1" : "0");
	context.write(" halted:");
	context.write(cpu.halted() ? "1" : "0");
	context.write(" stopped:");
	context.write(cpu.stopped() ? "1" : "0");
}

void writeCpuStep(lexxos::os::AppContext& context, std::uint32_t index, lexxos::gameboy::CpuStepResult const& step, lexxos::gameboy::Cpu const& cpu) {
	context.write("#");
	writeUnsigned(context, index);
	context.write(" pc=");
	writeHex16(context, step.pcBefore);
	context.write(" op=");
	writeCode8(context, step.opcode);
	if (step.opcode == 0xCB) {
		context.write(" cb=");
		writeCode8(context, step.prefixedOpcode);
	}
	context.write(" cycles=");
	writeUnsigned(context, step.cycles);
	context.write(" status=");
	context.write(lexxos::gameboy::toString(step.status));
	if (step.servicedInterrupt) {
		context.write(" irq=");
		writeCode8(context, step.interruptMask);
		context.write(" vector=");
		writeHex16(context, step.interruptVector);
	}
	context.write(" | ");
	writeCpuState(context, cpu);
	context.writeLine("");
}

void writeRunStopStep(lexxos::os::AppContext& context, char const* prefix, RunBudgetResult const& run) {
	if (!run.hasLastStep || isContinuableEmulationStatus(run.status)) {
		return;
	}
	context.write(prefix);
	context.write("stop pc=");
	writeHex16(context, run.lastStep.pcBefore);
	context.write(" op=");
	writeCode8(context, run.lastStep.opcode);
	if (run.lastStep.opcode == 0xCB) {
		context.write(" cb=");
		writeCode8(context, run.lastStep.prefixedOpcode);
	}
	context.write(" status=");
	context.write(lexxos::gameboy::toString(run.status));
	context.writeLine("");
}

void writeRunTrace(lexxos::os::AppContext& context, char const* prefix, RunTraceBuffer const& trace) {
	if (trace.count == 0) {
		return;
	}

	context.write(prefix);
	context.writeLine("last-opcodes:");
	auto const start = (trace.next + RunTraceBuffer::capacity - trace.count) % RunTraceBuffer::capacity;
	for (std::size_t offset = 0; offset < trace.count; ++offset) {
		auto const index = (start + offset) % RunTraceBuffer::capacity;
		auto const& entry = trace.entries[index];
		context.write(prefix);
		context.write("  pc=");
		writeHex16(context, entry.pc);
		context.write(" op=");
		writeCode8(context, entry.opcode);
		if (entry.opcode == 0xCB) {
			context.write(" cb=");
			writeCode8(context, entry.prefixedOpcode);
		}
		context.write(" cycles=");
		writeUnsigned(context, entry.cycles);
		if (entry.repeatCount > 1U) {
			context.write(" repeat=");
			writeUnsigned(context, entry.repeatCount);
			context.write(" total-cycles=");
			writeUnsigned(context, entry.totalCycles);
		}
		if (entry.servicedInterrupt) {
			context.write(" irq=");
			writeCode8(context, entry.interruptMask);
			context.write(" vector=");
			writeHex16(context, entry.interruptVector);
		}
		context.write(" bank=");
		writeUnsigned(context, entry.romBank);
		if (entry.pc < 0x8000U) {
			context.write(" rom=0x");
			std::uint32_t romAddress = entry.pc;
			if (entry.pc >= 0x4000U) {
				romAddress = entry.romBank * 0x4000U + static_cast<std::uint32_t>(entry.pc - 0x4000U);
			}
			writeHex32(context, romAddress);
		}
		context.write(" status=");
		context.write(lexxos::gameboy::toString(entry.status));
		context.writeLine("");
	}
}

void writeGbEmuFaultSnapshot(
    lexxos::os::AppContext& context,
    char const* reason,
    std::uint32_t frame,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cartridge const& cartridge,
    lexxos::gameboy::FrameBuffer const& framebuffer,
    lexxos::gameboy::BackgroundMapMode bgMapMode,
    lexxos::gameboy::BackgroundTileDataMode bgTileMode,
    RunTraceBuffer const& trace) {
	context.write("gbemu-debug: stop reason=");
	context.write(reason);
	context.write(" frame=");
	writeUnsigned(context, frame);
	context.writeLine("");
	context.write("gbemu-debug: ");
	writeCpuState(context, cpu);
	context.writeLine("");
	writeBusIoSnapshot(context, bus, "gbemu-debug: io ");
	writeVramRangeStats(context, bus);
	writeCartridgeCacheStats(context, cartridge, "gbemu-debug: ");
	writeBackgroundUsageStats(context, bus, framebuffer, bgMapMode, bgTileMode, "gbemu-debug: ");
	writeSpriteUsageStats(context, bus, "gbemu-debug: ");
	writeRunTrace(context, "gbemu-debug: ", trace);
}

} // namespace lexxos::apps::gameboy_programs
