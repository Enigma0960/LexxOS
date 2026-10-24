#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::app::picocalc_display_power {

struct ApplyResult {
	bool displayLockOk{false};
	bool displayOk{false};
	bool lcdBacklightAttempted{false};
	bool lcdBacklightOk{false};
	bool lcdBacklightValueValid{false};
	std::uint8_t lcdBacklightValue{0};
	bool keyboardBacklightAttempted{false};
	bool keyboardBacklightOk{false};
	bool keyboardBacklightValueValid{false};
	std::uint8_t keyboardBacklightValue{0};

	[[nodiscard]] bool ok() const {
		return displayOk
		    && (!lcdBacklightAttempted || lcdBacklightOk)
		    && (!keyboardBacklightAttempted || keyboardBacklightOk);
	}
};

struct IdleSettings {
	std::uint32_t timeoutMs{0};
	std::uint32_t dimTimeoutMs{0};
	std::uint8_t dimBacklight{26};
	lexxos::devices::DisplayPowerMode mode{lexxos::devices::DisplayPowerMode::Off};
	bool loadedFromConfig{false};
};

[[nodiscard]] std::string_view settingsPath();
[[nodiscard]] IdleSettings idleSettings(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool configureIdleTimeout(
    lexxos::platform::PlatformServices const& platform,
    std::uint32_t timeoutMs,
    lexxos::devices::DisplayPowerMode mode,
    bool save,
    IdleSettings* settings = nullptr);
void recordActivity(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool updateIdle(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform);

[[nodiscard]] bool applyPowerMode(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform,
    lexxos::devices::DisplayDevice& display,
    lexxos::devices::DisplayPowerMode mode,
    std::string_view owner,
    lexxos::devices::DeviceResourcePriority priority,
    ApplyResult* result = nullptr);

[[nodiscard]] bool wakeForInput(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform,
    std::string_view owner = "display-wake");

} // namespace lexxos::app::picocalc_display_power
