#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

enum PointingButton : std::uint8_t {
	PointingButtonLeft = 1U << 0U,
	PointingButtonRight = 1U << 1U,
	PointingButtonMiddle = 1U << 2U,
	PointingButtonBack = 1U << 3U,
	PointingButtonForward = 1U << 4U,
};

struct PointingDeviceState {
	bool connected{false};
	std::int32_t x{0};
	std::int32_t y{0};
	std::int32_t wheel{0};
	std::uint8_t buttons{0};
	std::uint32_t reportCount{0};
	std::uint32_t lastReportTimestampMs{0};

	[[nodiscard]] bool buttonPressed(PointingButton button) const {
		return (buttons & static_cast<std::uint8_t>(button)) != 0;
	}
};

inline constexpr std::size_t maxTouchContacts = 10;

struct TouchCalibration {
	bool valid{false};
	bool swapXY{false};
	bool invertX{false};
	bool invertY{false};
	std::int32_t rawMinX{0};
	std::int32_t rawMaxX{0};
	std::int32_t rawMinY{0};
	std::int32_t rawMaxY{0};
	std::int32_t displayWidth{0};
	std::int32_t displayHeight{0};
};

struct TouchContact {
	bool active{false};
	std::uint8_t id{0};
	std::int32_t x{0};
	std::int32_t y{0};
	std::int32_t rawX{0};
	std::int32_t rawY{0};
	std::uint16_t size{0};
};

struct TouchDeviceState {
	bool connected{false};
	std::uint8_t contactCount{0};
	std::uint8_t maxContacts{0};
	TouchContact contacts[maxTouchContacts]{};
	TouchCalibration calibration{};
	std::uint32_t reportCount{0};
	std::uint32_t lastReportTimestampMs{0};
};

class PointingDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override;
	[[nodiscard]] virtual PointingDeviceState state() const = 0;
	[[nodiscard]] virtual bool supportsTouch() const;
	[[nodiscard]] virtual TouchDeviceState touchState() const;
	[[nodiscard]] virtual TouchCalibration touchCalibration() const;
	virtual bool applyTouchCalibration(TouchCalibration const& calibration);
	virtual void resetPosition() = 0;
};

} // namespace lexxos::devices
