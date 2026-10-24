#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>

namespace lexxos::devices {

PicoCalcSouthbridgeRuntimeService* PicoCalcSouthbridgeResourcePolicy::service(DeviceManager& devices) {
	return devices.picoCalcSouthbridgeService(serviceName);
}

PicoCalcSouthbridgeRuntimeService* PicoCalcSouthbridgeResourcePolicy::readyService(DeviceManager& devices) {
	auto* runtimeService = service(devices);
	return runtimeService != nullptr && runtimeService->isReady() ? runtimeService : nullptr;
}

DeviceResourceLock PicoCalcSouthbridgeResourcePolicy::lock(
    DeviceManager& devices,
    std::string_view owner,
    DeviceResourcePriority priority) {
	return devices.lockPicoCalcSouthbridgeService(serviceName, priority, owner);
}

} // namespace lexxos::devices
