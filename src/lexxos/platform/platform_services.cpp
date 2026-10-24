#include <lexxos/platform/platform_services.h>

namespace {

constexpr std::string_view kGameBoyRomCacheRegionName{"gameboy-rom-cache"};
constexpr std::string_view kGameBoyRomPsramRegionName{"gameboy-rom-psram"};

bool findFlashRegion(std::string_view name, lexxos::platform::GameBoyRomFlashRegion& output) {
	if (name == kGameBoyRomCacheRegionName) {
		return lexxos::platform::gameBoyRomFlashRegion(output);
	}

	output = {};
	return false;
}

#if defined(LEXXOS_PLATFORM_PICO)
bool findExternalMemoryRegion(std::string_view name, lexxos::platform::GameBoyRomExternalMemoryRegion& output) {
	if (name == kGameBoyRomPsramRegionName) {
		return lexxos::platform::gameBoyRomPsramRegion(output);
	}

	output = {};
	return false;
}

bool readExternalMemoryRegion(std::string_view name, std::uint32_t offset, void* data, std::size_t size) {
	if (name == kGameBoyRomPsramRegionName) {
		return lexxos::platform::readGameBoyRomPsram(offset, data, size);
	}
	return false;
}

bool writeExternalMemoryRegion(
    std::string_view name,
    std::uint32_t offset,
    void const* data,
    std::size_t size) {
	if (name == kGameBoyRomPsramRegionName) {
		return lexxos::platform::writeGameBoyRomPsram(offset, data, size);
	}
	return false;
}
#endif

} // namespace

lexxos::platform::PlatformServices const& lexxos::platform::services() {
	static constexpr PlatformServices services{
	    .lifecycle = {
	                  .initialize = initialize,
	                  },
	    .board = {
	                  .current = board,
	                  },
	    .console = {
	                  .write = writeConsole,
	                  .readChar = readConsoleChar,
	                  .readAvailable = readConsoleAvailable,
	                  },
	    .clock = {
	                  .sleepMs = sleepMs,
	                  .uptimeMs = uptimeMs,
	                  },
	    .gpio = {
	                  .setStatusLed = setStatusLed,
	                  .configureInput = configureGpioInput,
	                  .configureOutput = configureGpioOutput,
	                  .read = readGpio,
	                  .write = writeGpio,
	                  .configureInterrupt = configureGpioInterrupt,
	                  .disableInterrupt = disableGpioInterrupt,
	                  },
	    .power = {
	                  .reboot = reboot,
#if defined(LEXXOS_PLATFORM_ESPIDF)
	                  .readBattery = readBatteryStatus,
#endif
	                  },
	    .flashRegions = {
	                  .findRegion = findFlashRegion,
	                  .eraseAndProgram = eraseAndProgramGameBoyRomFlash,
	                  },
#if defined(LEXXOS_PLATFORM_PICO)
	    .externalMemory = {
	                  .findRegion = findExternalMemoryRegion,
	                  .read = readExternalMemoryRegion,
	                  .write = writeExternalMemoryRegion,
	                  },
#endif
	};

	return services;
}
