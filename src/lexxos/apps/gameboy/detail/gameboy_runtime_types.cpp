#include <lexxos/apps/gameboy/detail/gameboy_runtime_types.h>

namespace lexxos::apps::gameboy_programs::detail {

void GbEmuProfileWindow::reset() {
	frames = 0;
	renderedFrames = 0;
	steps = 0;
	cycles = 0;
	wallMs = 0;
	inputMs = 0;
	cpuMs = 0;
	ppuMs = 0;
	blitMs = 0;
	cacheFills = 0;
	cacheRetries = 0;
	cacheFailures = 0;
	cacheReadBytes = 0;
	cacheFillMs = 0;
}

void RunTraceBuffer::push(lexxos::gameboy::CpuStepResult const& step, lexxos::gameboy::Cartridge const* cartridge) {
	if (count != 0 && step.status == lexxos::gameboy::CpuStepStatus::Halted) {
		auto const previousIndex = (next + capacity - 1U) % capacity;
		auto& previous = entries[previousIndex];
		if (previous.status == lexxos::gameboy::CpuStepStatus::Halted && previous.pc == step.pcBefore && !previous.servicedInterrupt) {
			++previous.repeatCount;
			previous.totalCycles += step.cycles;
			previous.cycles = step.cycles;
			return;
		}
	}

	entries[next] = RunTraceEntry{
	    .pc = step.pcBefore,
	    .opcode = step.opcode,
	    .prefixedOpcode = step.prefixedOpcode,
	    .cycles = step.cycles,
	    .totalCycles = step.cycles,
	    .repeatCount = 1,
	    .romBank = cartridge != nullptr ? static_cast<std::uint32_t>(cartridge->selectedRomBank()) : 0U,
	    .status = step.status,
	    .servicedInterrupt = step.servicedInterrupt,
	    .interruptMask = step.interruptMask,
	    .interruptVector = step.interruptVector,
	};
	next = (next + 1U) % capacity;
	if (count < capacity) {
		++count;
	}
}

} // namespace lexxos::apps::gameboy_programs::detail
