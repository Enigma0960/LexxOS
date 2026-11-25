#pragma once

#include <lexxos/core/core_context.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/lifecycle/event_bus.h>

namespace lexxos::core {

// Общий реестр Standard и CE. Инициализация/регистрация выполняются до запуска рабочих задач.
class CoreSystem final {
public:
	explicit CoreSystem(lexxos::platform::PlatformServices const& platform,
	    lexxos::lifecycle::EventBus& events = lexxos::lifecycle::defaultEventBus());
	CoreSystem(CoreSystem const&) = delete;
	CoreSystem& operator=(CoreSystem const&) = delete;

	// Сбрасывает реестр, но не переинициализирует оборудование и не очищает общую шину событий.
	void initialize();
	void refreshCapabilities();
	[[nodiscard]] CoreContext createContext();
	[[nodiscard]] lexxos::devices::DeviceManager& devices();
	[[nodiscard]] lexxos::devices::DeviceManager const& devices() const;
	[[nodiscard]] lexxos::platform::PlatformServices const& platform() const;
	[[nodiscard]] lexxos::lifecycle::EventBus& events() const;
	[[nodiscard]] lexxos::runtime::CapabilitySet capabilities() const;

private:
	lexxos::platform::PlatformServices const& _platform;
	lexxos::lifecycle::EventBus& _events;
	lexxos::devices::DeviceManager _devices{};
	lexxos::runtime::CapabilitySet _capabilities{};
};

} // namespace lexxos::core
