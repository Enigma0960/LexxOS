#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/gamepad_device.h>
#include <lexxos/devices/io/pcf8575.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices::input::detail {
struct SegaGenesisGamepadSample;
}

namespace lexxos::devices {

class SegaGenesisGamepad final : public GamepadDevice {
public:
	SegaGenesisGamepad(Pcf8575PortExpander& expander, lexxos::platform::SegaGenesisGamepadPins config);
	SegaGenesisGamepad(Pcf8575PortExpander& expander, lexxos::platform::SegaGenesisGamepadPins config, lexxos::platform::PlatformServices const& platform);

	bool initialize();
	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] GamepadState state() override;
	[[nodiscard]] bool isConnected();

private:
	[[nodiscard]] std::uint16_t _pinMask(std::uint8_t pin) const;
	[[nodiscard]] std::uint16_t _inputMask() const;
	[[nodiscard]] bool _isActive(std::uint16_t port, std::uint8_t pin) const;
	bool _readPhase(bool selectHigh, std::uint16_t& port);
	bool _readSample(input::detail::SegaGenesisGamepadSample& sample);
	[[nodiscard]] bool _hasThreeButtonSignature(std::uint16_t lowPhase) const;

	Pcf8575PortExpander& _expander;
	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::platform::SegaGenesisGamepadPins _config;
	bool _ready{false};
};

} // namespace lexxos::devices
