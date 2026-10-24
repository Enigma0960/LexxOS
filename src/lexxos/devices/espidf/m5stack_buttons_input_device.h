#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/input_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

struct M5StackButtonConfig {
	lexxos::platform::GpioPin pin;
	KeyCode key{KeyCode::Unknown};
	lexxos::platform::GpioActiveLevel activeLevel{lexxos::platform::GpioActiveLevel::Low};
};

struct M5StackButtonsInputConfig {
	std::string_view name{"m5stack-buttons"};
	M5StackButtonConfig const* buttons{nullptr};
	std::size_t buttonCount{0};
	std::uint32_t debounceMs{20};
};

class M5StackButtonsInputDevice final : public InputDevice {
public:
	static constexpr std::size_t maxButtons = 8;
	static constexpr std::size_t eventQueueCapacity = 16;

	explicit M5StackButtonsInputDevice(M5StackButtonsInputConfig config = {});
	M5StackButtonsInputDevice(M5StackButtonsInputConfig config, lexxos::platform::PlatformServices const& platform);

	bool initialize();
	void update() override;

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	bool pollEvent(InputEvent& event) override;
	[[nodiscard]] bool isPressed(KeyCode key) const override;
	[[nodiscard]] std::uint32_t rawState() const override;
	[[nodiscard]] std::uint32_t rawKeyCount() const override;

private:
	[[nodiscard]] bool _isConfigurationValid() const;
	[[nodiscard]] std::uint32_t _readRawState() const;
	void _applyStableState(std::uint32_t state);
	void _pushEvent(InputEventType type, KeyCode key, std::uint32_t timestampMs);
	[[nodiscard]] bool _popEvent(InputEvent& event);
	[[nodiscard]] std::uint32_t _uptimeMs() const;
	[[nodiscard]] std::uint32_t _pressedKeyMask(std::uint32_t rawState) const;
	[[nodiscard]] static std::uint32_t _keyMask(KeyCode key);

	lexxos::platform::PlatformServices const& _platformServices;
	M5StackButtonsInputConfig _config;
	bool _ready{false};
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
