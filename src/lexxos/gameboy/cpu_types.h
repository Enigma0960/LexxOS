#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::gameboy {

enum class CpuStepStatus : std::uint8_t {
	Ok,
	Halted,
	Stopped,
	UnsupportedOpcode,
	BusReadError,
};

std::string_view toString(CpuStepStatus status);

struct CpuStepResult {
	CpuStepStatus status{CpuStepStatus::Ok};
	std::uint16_t pcBefore{0};
	std::uint8_t opcode{0};
	std::uint8_t prefixedOpcode{0};
	std::uint8_t cycles{0};
	bool servicedInterrupt{false};
	std::uint8_t interruptMask{0};
	std::uint16_t interruptVector{0};
};

} // namespace lexxos::gameboy
