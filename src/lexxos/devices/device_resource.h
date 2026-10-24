#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::devices {

enum class DeviceResourcePriority : std::uint8_t {
	Background = 0,
	Normal = 1,
	Interactive = 2,
	System = 3,
	Critical = 4,
};

[[nodiscard]] constexpr std::string_view toString(DeviceResourcePriority priority) {
	switch (priority) {
		case DeviceResourcePriority::Background:
			return "background";
		case DeviceResourcePriority::Normal:
			return "normal";
		case DeviceResourcePriority::Interactive:
			return "interactive";
		case DeviceResourcePriority::System:
			return "system";
		case DeviceResourcePriority::Critical:
			return "critical";
	}
	return "normal";
}

struct DeviceResourceStatus {
	bool locked{false};
	DeviceResourcePriority priority{DeviceResourcePriority::Normal};
	std::string_view owner{};
};

} // namespace lexxos::devices
