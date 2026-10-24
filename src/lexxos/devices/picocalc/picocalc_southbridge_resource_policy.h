#pragma once

#include <string_view>

#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

class PicoCalcSouthbridgeRuntimeService;

class PicoCalcSouthbridgeResourcePolicy final {
public:
	static constexpr std::string_view serviceName{"picocalc-southbridge"};

	[[nodiscard]] static PicoCalcSouthbridgeRuntimeService* service(DeviceManager& devices);
	[[nodiscard]] static PicoCalcSouthbridgeRuntimeService* readyService(DeviceManager& devices);
	[[nodiscard]] static DeviceResourceLock lock(
	    DeviceManager& devices,
	    std::string_view owner,
	    DeviceResourcePriority priority);
};

} // namespace lexxos::devices
