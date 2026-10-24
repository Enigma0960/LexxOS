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

class CardputerKeyboard final : public InputDevice {
public:
	static constexpr std::size_t rowCount = 4;
	static constexpr std::size_t columnCount = 14;
	static constexpr std::size_t keyCount = rowCount * columnCount;
	static constexpr std::size_t eventQueueCapacity = 32;

	explicit CardputerKeyboard(lexxos::platform::PlatformServices const& platform);

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

	enum class Modifier : std::uint8_t {
		None,
		Fn,
		Shift,
		Ctrl,
		Opt,
		Alt,
	};

	struct KeyDefinition {
		KeyCode key{KeyCode::Unknown};
		char normal{0};
		char shifted{0};
		Modifier modifier{Modifier::None};
		KeyCode fnKey{KeyCode::Unknown};
	};

	struct InputPinMapping {
		lexxos::platform::GpioPin pin;
		std::uint8_t leftHalfColumn{0};
		std::uint8_t rightHalfColumn{0};
	};

private:
	[[nodiscard]] bool _configurePins();
	[[nodiscard]] std::uint64_t _readRawState() const;
	void _selectOutput(std::uint8_t selector) const;
	void _applyStableState(std::uint64_t state);
	void _updatePressedKeyCache(std::uint64_t state);
	void _pushEvent(InputEventType type, std::uint8_t row, std::uint8_t column, std::uint32_t timestampMs);
	[[nodiscard]] bool _popEvent(InputEvent& event);
	[[nodiscard]] std::uint8_t _modifiersForState(std::uint64_t state) const;
	[[nodiscard]] static std::uint8_t _scanCode(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static std::uint64_t _keyBit(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static bool _isPressed(std::uint64_t state, std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static KeyDefinition const& _keyAt(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static std::uint32_t _keyIndex(KeyCode key);
	[[nodiscard]] std::uint32_t _uptimeMs() const;

	lexxos::platform::PlatformServices const& _platformServices;
	bool _ready{false};
	std::uint32_t _lastRawChangeMs{0};
	std::uint64_t _lastRawState{0};
	std::uint64_t _stableState{0};
	std::uint8_t _stableModifiers{0};
	std::array<bool, 128> _pressedKeys{};
	std::array<InputEvent, eventQueueCapacity> _events{};
	std::size_t _eventHead{0};
	std::size_t _eventTail{0};
	std::size_t _eventCount{0};
};

} // namespace lexxos::devices
