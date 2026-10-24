#include <lexxos/os/operating_system.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::os {

OperatingSystem::OperatingSystem()
    : OperatingSystem(lexxos::platform::services()) {
}

OperatingSystem::OperatingSystem(lexxos::platform::PlatformServices const& platformServices)
    : OperatingSystem(platformServices, lexxos::vfs::defaultVfs()) {
}

OperatingSystem::OperatingSystem(lexxos::platform::PlatformServices const& platformServices, lexxos::vfs::Vfs& vfs)
    : _platformServices(&platformServices) {
	_vfs = &vfs;
}

void OperatingSystem::initialize() {
	_devices.initialize();
	refreshCapabilities();
}

void OperatingSystem::refreshCapabilities() {
	_capabilities = lexxos::runtime::detectCapabilities(_devices, platform());
}

AppContext OperatingSystem::createAppContext(AppContextCreateInfo const& info) {
	return AppContext{*this, info};
}

lexxos::program::ProgramManager& OperatingSystem::programs() {
	return _programs;
}

lexxos::program::ProgramManager const& OperatingSystem::programs() const {
	return _programs;
}

lexxos::devices::DeviceManager& OperatingSystem::devices() {
	return _devices;
}

lexxos::devices::DeviceManager const& OperatingSystem::devices() const {
	return _devices;
}

lexxos::vfs::Vfs& OperatingSystem::vfs() {
	return *_vfs;
}

lexxos::vfs::Vfs const& OperatingSystem::vfs() const {
	return *_vfs;
}

lexxos::platform::PlatformServices const& OperatingSystem::platform() const {
	return *_platformServices;
}

lexxos::runtime::RuntimeProfile const& OperatingSystem::runtimeProfile() const {
	return lexxos::runtime::runtimeProfile();
}

lexxos::runtime::CapabilitySet OperatingSystem::capabilities() const {
	return _capabilities;
}

} // namespace lexxos::os
