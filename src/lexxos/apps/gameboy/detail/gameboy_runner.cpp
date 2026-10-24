#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

bool isContinuableEmulationStatus(lexxos::gameboy::CpuStepStatus status) {
	return status == lexxos::gameboy::CpuStepStatus::Ok || status == lexxos::gameboy::CpuStepStatus::Halted;
}

RunBudgetResult runCpuBudget(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t stepLimit,
    std::uint32_t cycleLimit,
    RunTraceBuffer* trace,
    lexxos::gameboy::Cartridge const* cartridge,
    std::uint16_t const* breakPc,
    std::uint16_t const* breakPcFrom,
    bool fastHalt,
    bool fastTiming) {
	RunBudgetResult result{};
	for (std::uint32_t stepIndex = 0; stepIndex < stepLimit; ++stepIndex) {
		if (cycleLimit != 0 && result.cycles >= cycleLimit) {
			break;
		}

		lexxos::gameboy::CpuStepResult step{};
		if (fastHalt && cycleLimit != 0 && cpu.halted() && bus.pendingInterrupts() == 0) {
			auto fastCycles = cycleLimit - result.cycles;
			if (fastCycles > fastHaltStepCycles) {
				fastCycles = fastHaltStepCycles;
			}
			fastCycles -= fastCycles % 4U;
			if (fastCycles >= 4U) {
				step.status = lexxos::gameboy::CpuStepStatus::Halted;
				step.pcBefore = cpu.pc();
				step.cycles = static_cast<std::uint8_t>(fastCycles);
			} else {
				step = cpu.step(bus);
			}
		} else {
			step = cpu.step(bus);
		}
		if (trace != nullptr) {
			trace->push(step, cartridge);
		}
		if (fastTiming) {
			bus.advanceCyclesFast(step.cycles);
		} else {
			bus.advanceCycles(step.cycles);
		}
		result.cycles += step.cycles;
		++result.steps;
		result.status = step.status;
		result.lastStep = step;
		result.hasLastStep = true;

		if (breakPc != nullptr && cpu.pc() == *breakPc) {
			result.breakpointHit = true;
			break;
		}

		if (breakPcFrom != nullptr && cpu.pc() >= *breakPcFrom) {
			result.pcRangeHit = true;
			break;
		}

		if (!isContinuableEmulationStatus(step.status)) {
			break;
		}
	}
	return result;
}

RunBudgetResult runUntilVramWrite(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    RunTraceBuffer* trace,
    lexxos::gameboy::Cartridge const* cartridge,
    std::uint16_t const* breakPc,
    bool fastHalt) {
	RunBudgetResult total{};
	auto const initialVramWrites = bus.vramWriteCount();
	while (total.cycles < cycleLimit && bus.vramWriteCount() == initialVramWrites) {
		auto const chunk = runCpuBudget(cpu, bus, 256, 4096, trace, cartridge, breakPc, nullptr, fastHalt);
		total.steps += chunk.steps;
		total.cycles += chunk.cycles;
		total.status = chunk.status;
		total.lastStep = chunk.lastStep;
		total.hasLastStep = chunk.hasLastStep;
		if (chunk.breakpointHit) {
			total.breakpointHit = true;
		}
		if (chunk.pcRangeHit) {
			total.pcRangeHit = true;
		}
		if (chunk.breakpointHit || chunk.pcRangeHit || !isContinuableEmulationStatus(chunk.status) || chunk.steps == 0) {
			break;
		}
	}
	return total;
}

RunBudgetResult runUntilVisibleVram(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    RunTraceBuffer* trace,
    lexxos::gameboy::Cartridge const* cartridge,
    std::uint16_t const* breakPc) {
	RunBudgetResult total{};
	while (total.cycles < cycleLimit && bus.nonZeroVramBytes() == 0) {
		auto const chunk = runCpuBudget(cpu, bus, 512, 8192, trace, cartridge, breakPc);
		total.steps += chunk.steps;
		total.cycles += chunk.cycles;
		total.status = chunk.status;
		total.lastStep = chunk.lastStep;
		total.hasLastStep = chunk.hasLastStep;
		if (chunk.breakpointHit) {
			total.breakpointHit = true;
		}
		if (chunk.pcRangeHit) {
			total.pcRangeHit = true;
		}
		if (chunk.breakpointHit || chunk.pcRangeHit || !isContinuableEmulationStatus(chunk.status) || chunk.steps == 0) {
			break;
		}
	}
	return total;
}

RunBudgetResult runUntilVramTarget(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    VramWarmupTarget target,
    std::uint32_t minTarget,
    RunTraceBuffer* trace,
    lexxos::gameboy::Cartridge const* cartridge,
    std::uint16_t const* breakPc,
    bool fastHalt) {
	if (minTarget == 0) {
		minTarget = 1;
	}

	RunBudgetResult total{};
	while (total.cycles < cycleLimit && countNonZeroForTarget(bus, target) < minTarget) {
		auto const chunk = runCpuBudget(cpu, bus, 4096, 65536, trace, cartridge, breakPc, nullptr, fastHalt);
		total.steps += chunk.steps;
		total.cycles += chunk.cycles;
		total.status = chunk.status;
		total.lastStep = chunk.lastStep;
		total.hasLastStep = chunk.hasLastStep;
		if (chunk.breakpointHit) {
			total.breakpointHit = true;
		}
		if (chunk.pcRangeHit) {
			total.pcRangeHit = true;
		}
		if (chunk.breakpointHit || chunk.pcRangeHit || !isContinuableEmulationStatus(chunk.status) || chunk.steps == 0) {
			break;
		}
	}
	return total;
}

RunBudgetResult runUntilBgReady(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    std::uint32_t minTileData,
    RunTraceBuffer* trace,
    lexxos::gameboy::Cartridge const* cartridge,
    std::uint16_t const* breakPc,
    bool fastHalt) {
	if (minTileData == 0) {
		minTileData = 1;
	}

	RunBudgetResult total{};
	while (total.cycles < cycleLimit) {
		auto const tileData = countNonZeroForTarget(bus, VramWarmupTarget::TileData);
		auto const mapData = countNonZeroForTarget(bus, VramWarmupTarget::Map);
		if (tileData >= minTileData && mapData > 0) {
			break;
		}

		auto const chunk = runCpuBudget(cpu, bus, 4096, 65536, trace, cartridge, breakPc, nullptr, fastHalt);
		total.steps += chunk.steps;
		total.cycles += chunk.cycles;
		total.status = chunk.status;
		total.lastStep = chunk.lastStep;
		total.hasLastStep = chunk.hasLastStep;
		if (chunk.breakpointHit) {
			total.breakpointHit = true;
		}
		if (chunk.pcRangeHit) {
			total.pcRangeHit = true;
		}
		if (chunk.breakpointHit || chunk.pcRangeHit || !isContinuableEmulationStatus(chunk.status) || chunk.steps == 0) {
			break;
		}
	}
	return total;
}

} // namespace lexxos::apps::gameboy_programs
