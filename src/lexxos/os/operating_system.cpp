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
    : OperatingSystem(platformServices, vfs, app::program_input::defaultService(), app::console_input::defaultService()) {
}

OperatingSystem::OperatingSystem(platform::PlatformServices const& platformServices, lexxos::vfs::Vfs& vfs,
    app::ProgramInputService& programInput, app::ConsoleInputService& consoleInput)
    : _core(platformServices, programInput.events())
    , _vfs(&vfs)
    , _programInput(programInput)
    , _consoleInput(consoleInput) {
}

app::ProgramInputService& OperatingSystem::programInput() {
	return _programInput;
}

app::ConsoleInputService& OperatingSystem::consoleInput() {
	return _consoleInput;
}

void OperatingSystem::initialize() {
	_core.initialize();
}

void OperatingSystem::refreshCapabilities() {
	_core.refreshCapabilities();
}

lexxos::core::CoreSystem& OperatingSystem::core() {
	return _core;
}

lexxos::core::CoreSystem const& OperatingSystem::core() const {
	return _core;
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
	return _core.devices();
}

lexxos::devices::DeviceManager const& OperatingSystem::devices() const {
	return _core.devices();
}

lexxos::vfs::Vfs& OperatingSystem::vfs() {
	return *_vfs;
}

lexxos::vfs::Vfs const& OperatingSystem::vfs() const {
	return *_vfs;
}

lexxos::platform::PlatformServices const& OperatingSystem::platform() const {
	return _core.platform();
}

lexxos::runtime::RuntimeProfile const& OperatingSystem::runtimeProfile() const {
	return lexxos::runtime::runtimeProfile();
}

lexxos::runtime::CapabilitySet OperatingSystem::capabilities() const {
	return _core.capabilities();
}

StorageMountPolicy* OperatingSystem::storageMountPolicy() const {
	return _storageMountPolicy;
}

void OperatingSystem::setStorageMountPolicy(StorageMountPolicy* policy) {
	_storageMountPolicy = policy;
}

} // namespace lexxos::os
