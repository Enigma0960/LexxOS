#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/input_device.h>
#include <lexxos/platform/platform.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

struct MatrixKeyboardConfig {
	std::string_view name{"matrix-keyboard"};
	lexxos::platform::GpioPin const* rows{nullptr};
	std::size_t rowCount{0};
	lexxos::platform::GpioPin const* columns{nullptr};
	std::size_t columnCount{0};
	KeyCode const* keyMap{nullptr};
	std::size_t keyMapCount{0};
	lexxos::platform::GpioActiveLevel rowActiveLevel{lexxos::platform::GpioActiveLevel::Low};
	lexxos::platform::GpioActiveLevel columnActiveLevel{lexxos::platform::GpioActiveLevel::Low};
	std::uint32_t scanPeriodMs{5};
	std::uint32_t debounceMs{20};
};

class MatrixKeyboard final : public InputDevice {
public:
	static constexpr std::size_t maxKeys = 32;
	static constexpr std::size_t eventQueueCapacity = 16;

	explicit MatrixKeyboard(MatrixKeyboardConfig config);
	MatrixKeyboard(MatrixKeyboardConfig config, lexxos::platform::PlatformServices const& platform);

	bool initialize();
	void update() override;

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	bool pollEvent(InputEvent& event) override;
	[[nodiscard]] bool isPressed(KeyCode key) const override;
	[[nodiscard]] std::uint32_t rawState() const override;
	[[nodiscard]] std::uint32_t rawKeyCount() const override;
	[[nodiscard]] std::uint32_t rawRowCount() const override;
	[[nodiscard]] std::uint32_t rawColumnCount() const override;

private:
	[[nodiscard]] bool _isConfigurationValid() const;
	[[nodiscard]] std::size_t _keyCount() const;
	[[nodiscard]] std::uint32_t _readRawState();
	void _driveAllRowsInactive();
	void _driveRowActive(std::size_t row);
	[[nodiscard]] bool _readColumnActive(std::size_t column) const;
	void _applyStableState(std::uint32_t state);
	void _pushEvent(InputEventType type, KeyCode key, std::uint32_t timestampMs);
	[[nodiscard]] bool _popEvent(InputEvent& event);
	[[nodiscard]] std::uint32_t _uptimeMs() const;
	[[nodiscard]] static bool _activeLevelToValue(lexxos::platform::GpioActiveLevel level);
	[[nodiscard]] static std::uint32_t _keyMask(KeyCode key);

	lexxos::platform::PlatformServices const& _platformServices;
	MatrixKeyboardConfig _config;
	bool _ready{false};
	bool _firstScan{true};
	std::uint32_t _lastScanMs{0};
	std::uint32_t _lastRawChangeMs{0};
	std::uint32_t _lastRawState{0};
	std::uint32_t _stableState{0};
	std::uint32_t _pressedKeys{0};
	std::array<InputEvent, eventQueueCapacity> _events{};
	std::size_t _eventHead{0};
	std::size_t _eventTail{0};
	std::size_t _eventCount{0};
};

} // namespace lexxos::devices
