#include <lexxos/core/core_system.h>

namespace lexxos::core {

CoreSystem::CoreSystem(lexxos::platform::PlatformServices const& platform, lexxos::lifecycle::EventBus& events)
    : _platform(platform)
    , _events(events) {
}

void CoreSystem::initialize() {
	_devices.initialize(_platform);
	refreshCapabilities();
}

void CoreSystem::refreshCapabilities() {
	_capabilities = lexxos::runtime::detectCapabilities(_devices, _platform);
}

CoreContext CoreSystem::createContext() {
	return CoreContext{*this};
}

lexxos::devices::DeviceManager& CoreSystem::devices() {
	return _devices;
}

lexxos::devices::DeviceManager const& CoreSystem::devices() const {
	return _devices;
}

lexxos::platform::PlatformServices const& CoreSystem::platform() const {
	return _platform;
}

lexxos::lifecycle::EventBus& CoreSystem::events() const {
	return _events;
}

lexxos::runtime::CapabilitySet CoreSystem::capabilities() const {
	return _capabilities;
}

} // namespace lexxos::core
