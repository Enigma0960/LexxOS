#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <lexxos/platform/platform.h>

namespace lexxos::devices {

class I2cBus;
class SpiController;
class AnalogInput;
class AnalogOutput;
class PwmOutput;

} // namespace lexxos::devices

namespace lexxos::platform {

struct PlatformLifecycleService {
	void (*initialize)(){nullptr};
};

struct PlatformBoardService {
	LegacyBoardDescriptor const& (*current)(){nullptr};
};

struct PlatformConsoleService {
	void (*write)(std::string_view text){nullptr};
	bool (*readChar)(char& value){nullptr};
	std::size_t (*readAvailable)(char* buffer, std::size_t capacity){nullptr};
	// Optional byte-exact output for consoles whose text writer translates newlines.
	void (*writeRaw)(std::string_view bytes){nullptr};
};

struct PlatformClockService {
	void (*sleepMs)(std::uint32_t milliseconds){nullptr};
	std::uint32_t (*uptimeMs)(){nullptr};
};

struct PlatformGpioService {
	void (*setStatusLed)(bool enabled){nullptr};
	bool (*configureInput)(GpioPin pin, GpioPull pull){nullptr};
	bool (*configureOutput)(GpioPin pin, bool initialValue){nullptr};
	bool (*read)(GpioPin pin, bool& value){nullptr};
	bool (*write)(GpioPin pin, bool value){nullptr};
	bool (*configureInterrupt)(GpioPin pin, GpioInterruptEdge edge, GpioInterruptHandler handler, void* userData){nullptr};
	void (*disableInterrupt)(GpioPin pin){nullptr};
};

struct PlatformPowerService {
	void (*reboot)(){nullptr};
	bool (*readBattery)(PlatformBatteryStatus& output){nullptr};
};

struct PlatformFlashRegionService {
	bool (*findRegion)(std::string_view name, GameBoyRomFlashRegion& output){nullptr};
	bool (*eraseAndProgram)(std::uint32_t offset, void const* data, std::size_t size){nullptr};
};

struct PlatformExternalMemoryRegionService {
	bool (*findRegion)(std::string_view name, GameBoyRomExternalMemoryRegion& output){nullptr};
	bool (*read)(std::string_view name, std::uint32_t offset, void* data, std::size_t size){nullptr};
	bool (*write)(std::string_view name, std::uint32_t offset, void const* data, std::size_t size){nullptr};
};

struct PlatformServices {
	PlatformLifecycleService lifecycle{};
	PlatformBoardService board{};
	PlatformConsoleService console{};
	PlatformClockService clock{};
	PlatformGpioService gpio{};
	PlatformPowerService power{};
	PlatformFlashRegionService flashRegions{};
	PlatformExternalMemoryRegionService externalMemory{};
	std::unique_ptr<devices::I2cBus> (*createI2cBus)(){nullptr};
	std::unique_ptr<devices::SpiController> (*createSpiController)(){nullptr};
	std::unique_ptr<devices::AnalogInput> (*createAnalogInput)(AnalogInputConfig const&){nullptr};
	std::unique_ptr<devices::PwmOutput> (*createPwmOutput)(PwmOutputConfig const&){nullptr};
	std::unique_ptr<devices::AnalogOutput> (*createAnalogOutput)(AnalogOutputConfig const&){nullptr};
};

[[nodiscard]] PlatformServices const& services();

} // namespace lexxos::platform
