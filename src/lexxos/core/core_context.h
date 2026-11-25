#pragma once

#include <lexxos/runtime/capabilities.h>

namespace lexxos::devices {

class DeviceManager;

} // namespace lexxos::devices

namespace lexxos::lifecycle {

class EventBus;

} // namespace lexxos::lifecycle

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::core {

class CoreSystem;

// Заимствованный контекст: ядро и его платформенные сервисы должны жить дольше приложения.
class CoreContext final {
public:
	explicit CoreContext(CoreSystem& system);
	[[nodiscard]] lexxos::devices::DeviceManager& devices() const;
	[[nodiscard]] lexxos::platform::PlatformServices const& platform() const;
	[[nodiscard]] lexxos::lifecycle::EventBus& events() const;
	[[nodiscard]] lexxos::runtime::CapabilitySet capabilities() const;

private:
	CoreSystem& _system;
};

} // namespace lexxos::core
