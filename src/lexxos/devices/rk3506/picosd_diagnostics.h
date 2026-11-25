#pragma once
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_profile_ro.h>

namespace lexxos::platform::rk3506 {

struct PicoSdDiagnosticStatus {
	bool configured;
	bool ready;
	bool highCapacity;
	std::uint32_t ocr;
	std::uint64_t sectorCount;
	char const* step;
	std::uint8_t response;
	bool requestSpi1;
	bool spi1Active;
	bool spi1Fault;
	bool csAsserted;
	PicoSdCrcMode crcMode;
};

struct PicoSdRegisterStatus {
	std::uint32_t gpioGate, spiGate, iocC, iocD;
	bool detectReadable, detectHigh;
};

PicoSdDiagnosticStatus picoSdDiagnosticStatus();
PicoSdRegisterStatus picoSdRegisterStatus();
bool picoSdSelectTransport(bool spi1);
bool picoSdProbeManual();

} // namespace lexxos::platform::rk3506
