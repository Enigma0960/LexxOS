#pragma once

#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/runtime/runtime_profile.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::os {

class OperatingSystem {
public:
	OperatingSystem();
	explicit OperatingSystem(lexxos::platform::PlatformServices const& platformServices);
	OperatingSystem(lexxos::platform::PlatformServices const& platformServices, lexxos::vfs::Vfs& vfs);

	void initialize();
	void refreshCapabilities();

	[[nodiscard]] AppContext createAppContext(AppContextCreateInfo const& info);
	[[nodiscard]] lexxos::program::ProgramManager& programs();
	[[nodiscard]] lexxos::program::ProgramManager const& programs() const;
	[[nodiscard]] lexxos::devices::DeviceManager& devices();
	[[nodiscard]] lexxos::devices::DeviceManager const& devices() const;
	[[nodiscard]] lexxos::vfs::Vfs& vfs();
	[[nodiscard]] lexxos::vfs::Vfs const& vfs() const;
	[[nodiscard]] lexxos::platform::PlatformServices const& platform() const;
	[[nodiscard]] lexxos::runtime::RuntimeProfile const& runtimeProfile() const;
	[[nodiscard]] lexxos::runtime::CapabilitySet capabilities() const;

private:
	lexxos::platform::PlatformServices const* _platformServices{nullptr};
	lexxos::vfs::Vfs* _vfs{nullptr};
	lexxos::program::ProgramManager _programs{};
	lexxos::devices::DeviceManager _devices{};
	lexxos::runtime::CapabilitySet _capabilities{};
};

} // namespace lexxos::os
