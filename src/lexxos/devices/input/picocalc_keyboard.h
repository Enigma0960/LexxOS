#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/input/detail/picocalc_keyboard_controller.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class I2cBus;

class PicoCalcKeyboard final : public InputDevice {
public:
	static constexpr std::uint8_t defaultAddress = 0x1F;
	static constexpr std::size_t eventQueueCapacity = input_detail::PicoCalcKeyboardController::eventQueueCapacity;

	PicoCalcKeyboard(I2cBus& bus, lexxos::platform::PlatformServices const& platform, std::uint8_t address = defaultAddress);

	bool initialize();
	void update() override;
	void setRawEventReporting(bool enabled) override;

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	bool pollEvent(InputEvent& event) override;
	[[nodiscard]] bool isPressed(KeyCode key) const override;
	[[nodiscard]] PicoCalcSouthbridgeRuntimeService& southbridgeRuntimeService();
	[[nodiscard]] PicoCalcSouthbridgeRuntimeService const& southbridgeRuntimeService() const;
	[[nodiscard]] std::uint32_t rawState() const override;
	[[nodiscard]] std::uint32_t rawKeyCount() const override;
	[[nodiscard]] std::uint32_t rawRowCount() const override;
	[[nodiscard]] std::uint32_t rawColumnCount() const override;

	[[nodiscard]] std::uint32_t staleScanCodeReleases() const;
	[[nodiscard]] std::uint32_t staleReleaseCount() const;

private:
	input_detail::PicoCalcKeyboardController _controller;
};

} // namespace lexxos::devices
