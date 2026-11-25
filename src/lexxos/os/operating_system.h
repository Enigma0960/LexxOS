#pragma once

#include <lexxos/app/console_input_service.h>
#include <lexxos/app/program_input_service.h>
#include <lexxos/core/core_system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/runtime/selected_runtime_profile.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::os {

class StorageMountPolicy;

class OperatingSystem {
public:
	OperatingSystem();
	explicit OperatingSystem(lexxos::platform::PlatformServices const& platformServices);
	OperatingSystem(lexxos::platform::PlatformServices const& platformServices, lexxos::vfs::Vfs& vfs);

	// Сервисы принадлежат composition и должны пережить OS и её задачи.
	OperatingSystem(lexxos::platform::PlatformServices const& platformServices, lexxos::vfs::Vfs& vfs,
	    app::ProgramInputService& programInput, app::ConsoleInputService& consoleInput);
	[[nodiscard]] app::ProgramInputService& programInput();
	[[nodiscard]] app::ConsoleInputService& consoleInput();

	void initialize();
	void refreshCapabilities();
	[[nodiscard]] lexxos::core::CoreSystem& core();
	[[nodiscard]] lexxos::core::CoreSystem const& core() const;

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

	[[nodiscard]] StorageMountPolicy* storageMountPolicy() const;
	void setStorageMountPolicy(StorageMountPolicy* policy);

private:
	StorageMountPolicy* _storageMountPolicy{nullptr};
	lexxos::core::CoreSystem _core;
	lexxos::vfs::Vfs* _vfs{nullptr};
	lexxos::program::ProgramManager _programs{};
	app::ProgramInputService& _programInput;
	app::ConsoleInputService& _consoleInput;
};

} // namespace lexxos::os
