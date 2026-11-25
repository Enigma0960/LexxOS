#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

struct SdGpioRegisters {
	std::uint32_t gpioGate, spiGate, iocC, iocD;
	bool detectReadable, detectHigh;
};

// Fixed, audited removable socket route. No protocol/media state or SDMMC.
void sdGpioConfigure();
void sdGpioChipSelect(bool high);
std::uint8_t sdGpioTransfer(std::uint8_t output);
void sdGpioEnableClock();
bool sdGpioCardInserted(); // Caller ensures GPIO clock is enabled.
SdGpioRegisters sdGpioRegisters(bool configured);

} // namespace lexxos::platform::rk3506
