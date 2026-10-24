#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cartridge.h>
#include <lexxos/gameboy/cpu.h>
#include <lexxos/gameboy/ppu.h>

namespace lexxos::apps::gameboy_programs::detail {

struct GameBoyWorkspace {
	lexxos::gameboy::Bus bus;
	lexxos::gameboy::Cpu cpu;
	lexxos::gameboy::FrameBuffer framebuffer;
	lexxos::gameboy::Ppu ppu;
	lexxos::gameboy::Cartridge cartridge;
};

struct RunBudgetResult {
	std::uint32_t steps{0};
	std::uint32_t cycles{0};
	lexxos::gameboy::CpuStepStatus status{lexxos::gameboy::CpuStepStatus::Ok};
	lexxos::gameboy::CpuStepResult lastStep{};
	bool hasLastStep{false};
	bool breakpointHit{false};
	bool pcRangeHit{false};
};

struct GbEmuProfileWindow {
	std::uint32_t frames{0};
	std::uint32_t renderedFrames{0};
	std::uint32_t steps{0};
	std::uint32_t cycles{0};
	std::uint32_t wallMs{0};
	std::uint32_t inputMs{0};
	std::uint32_t cpuMs{0};
	std::uint32_t ppuMs{0};
	std::uint32_t blitMs{0};
	std::uint32_t cacheFills{0};
	std::uint32_t cacheRetries{0};
	std::uint32_t cacheFailures{0};
	std::uint64_t cacheReadBytes{0};
	std::uint32_t cacheFillMs{0};

	void reset();
};

struct RunTraceEntry {
	std::uint16_t pc{0};
	std::uint8_t opcode{0};
	std::uint8_t prefixedOpcode{0};
	std::uint8_t cycles{0};
	std::uint32_t totalCycles{0};
	std::uint32_t repeatCount{1};
	std::uint32_t romBank{0};
	lexxos::gameboy::CpuStepStatus status{lexxos::gameboy::CpuStepStatus::Ok};
	bool servicedInterrupt{false};
	std::uint8_t interruptMask{0};
	std::uint16_t interruptVector{0};
};

struct RunTraceBuffer {
	static constexpr std::size_t capacity = 64;

	std::array<RunTraceEntry, capacity> entries{};
	std::size_t next{0};
	std::size_t count{0};

	void push(lexxos::gameboy::CpuStepResult const& step, lexxos::gameboy::Cartridge const* cartridge);
};

struct GbTestRunSummary {
	RunBudgetResult total{};
	bool opened{false};
	bool mapperSupported{false};
	bool passed{false};
	bool failed{false};
	std::uint32_t serialSize{0};
	bool serialOverflow{false};
	std::uint32_t cacheFills{0};
	std::uint32_t cacheRetries{0};
	std::uint32_t cacheFailures{0};
	std::uint32_t selectedBank{0};
};

} // namespace lexxos::apps::gameboy_programs::detail
