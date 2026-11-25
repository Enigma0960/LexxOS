#pragma once

#include <cstdint>

namespace lexxos::hardware::modules::lyra {

// Facts used by the current Lyra profile. PCB revision remains unverified.
inline constexpr char vendor[] = "Luckfox";
inline constexpr char processorName[] = "Rockchip RK3506G2 (Cortex-A7, CPU0)";
inline constexpr int bootConsoleIndex = 0;
inline constexpr std::uint32_t bootConsoleBaudRate = 1500000;

} // namespace lexxos::hardware::modules::lyra
