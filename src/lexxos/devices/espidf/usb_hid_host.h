#pragma once

#include <cstdint>

namespace lexxos::devices {
class GamepadDevice;
class InputDevice;
class PointingDevice;
} // namespace lexxos::devices

namespace lexxos::devices::espidf_usb_hid {

struct UsbHidHostStatus {
	bool initialized{false};
	bool usbHostInstalled{false};
	bool hidDriverInstalled{false};
	bool keyboardConnected{false};
	bool mouseConnected{false};
	bool gamepadConnected{false};
	bool usbInfoAvailable{false};
	bool rootPortSuspended{false};
	std::uint32_t usbDeviceCount{0};
	std::uint32_t usbClientCount{0};
	std::int32_t usbInfoError{0};
	std::uint32_t connectionEvents{0};
	std::uint32_t droppedEvents{0};
	std::uint32_t errors{0};
	std::int32_t lastError{0};
	std::uint32_t gamepadReports{0};
	std::uint32_t gamepadLastReportTimestampMs{0};
	std::uint8_t gamepadLastReportBytes{0};
	std::uint8_t gamepadReportId{0};
};

[[nodiscard]] bool initialize();
[[nodiscard]] InputDevice& keyboardDevice();
[[nodiscard]] PointingDevice& mouseDevice();
[[nodiscard]] GamepadDevice& gamepadDevice();
[[nodiscard]] UsbHidHostStatus status();

} // namespace lexxos::devices::espidf_usb_hid
