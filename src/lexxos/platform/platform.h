#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/platform/board.h>

namespace lexxos::platform {

enum class GpioInterruptEdge : std::uint8_t {
	Rising,
	Falling,
	Both,
};

using GpioInterruptHandler = void (*)(GpioPin pin, bool value, void* userData);

void initialize();
BoardDescriptor const& board();
void writeConsole(std::string_view text);
bool readConsoleChar(char& value);
#if defined(LEXXOS_PLATFORM_HOST)
bool isConsoleInteractive();
#endif
std::size_t readConsoleAvailable(char* buffer, std::size_t capacity);
void sleepMs(std::uint32_t milliseconds);
std::uint32_t uptimeMs();
void setStatusLed(bool enabled);
bool configureGpioInput(GpioPin pin, GpioPull pull = GpioPull::None);
bool configureGpioOutput(GpioPin pin, bool initialValue = false);
bool readGpio(GpioPin pin, bool& value);
bool writeGpio(GpioPin pin, bool value);
bool configureGpioInterrupt(GpioPin pin, GpioInterruptEdge edge, GpioInterruptHandler handler, void* userData);
void disableGpioInterrupt(GpioPin pin);
void reboot();

struct GameBoyRomFlashRegion {
	bool available{false};
	std::uint32_t offset{0};
	std::uint32_t capacity{0};
	std::uint32_t eraseSize{0};
	std::uint32_t programSize{0};
	std::uint8_t const* xipBase{nullptr};
};

struct GameBoyRomExternalMemoryRegion {
	bool available{false};
	std::uint32_t capacity{0};
	std::uint32_t preferredTransferBytes{0};
};

struct GameBoyRomPsramRuntimeInfo {
	bool available{false};
	std::uint32_t configuredBaudRate{0};
	std::uint32_t effectiveBaudRate{0};
	char const* backend{"none"};
	char const* lastPioFailure{"none"};
	bool pioConfigured{false};
	int pioIndex{-1};
	int stateMachine{-1};
	std::uint32_t transferCount{0};
	std::uint32_t pioTransferCount{0};
	std::uint32_t softwareTransferCount{0};
	std::uint32_t pioRejectedLowBaudCount{0};
	std::uint32_t pioConfigureFailureCount{0};
	std::uint32_t lastTransferBytes{0};
	std::uint32_t pioTransferTimeoutCount{0};
};

struct ClockRate {
	bool available{false};
	std::uint32_t hz{0};
};

struct PlatformPerformanceSnapshot {
	ClockRate systemClock{};
	ClockRate peripheralClock{};
	ClockRate referenceClock{};
	ClockRate usbClock{};
	ClockRate adcClock{};
	ClockRate rtcClock{};
};

struct PlatformBatteryStatus {
	bool available{false};
	bool voltageMvValid{false};
	std::uint16_t voltageMv{0};
	bool percentValid{false};
	std::uint8_t percent{0};
	bool chargingValid{false};
	bool charging{false};
	char const* source{"none"};
};

[[nodiscard]] PlatformPerformanceSnapshot performanceSnapshot();
[[nodiscard]] bool readBatteryStatus(PlatformBatteryStatus& output);
[[nodiscard]] bool gameBoyRomFlashRegion(GameBoyRomFlashRegion& output);
[[nodiscard]] bool eraseAndProgramGameBoyRomFlash(std::uint32_t offset, void const* data, std::size_t size);
[[nodiscard]] bool gameBoyRomPsramRegion(GameBoyRomExternalMemoryRegion& output);
[[nodiscard]] bool readGameBoyRomPsram(std::uint32_t offset, void* data, std::size_t size);
[[nodiscard]] bool writeGameBoyRomPsram(std::uint32_t offset, void const* data, std::size_t size);
[[nodiscard]] bool gameBoyRomPsramRuntimeInfo(GameBoyRomPsramRuntimeInfo& output);

} // namespace lexxos::platform
