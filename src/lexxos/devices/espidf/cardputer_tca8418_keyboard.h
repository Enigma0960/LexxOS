#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/input_device.h>

namespace lexxos::devices {

class I2cBus;

class CardputerTca8418Keyboard final : public InputDevice {
public:
	static constexpr std::size_t rowCount = 4;
	static constexpr std::size_t columnCount = 14;
	static constexpr std::size_t keyCount = rowCount * columnCount;
	static constexpr std::size_t eventQueueCapacity = 32;

	explicit CardputerTca8418Keyboard(I2cBus& bus, std::uint8_t address = 0x34);

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

private:
	struct RawKeyEvent {
		bool valid{false};
		bool pressed{false};
		std::uint8_t row{0};
		std::uint8_t column{0};
	};

	[[nodiscard]] bool _writeRegister(std::uint8_t reg, std::uint8_t value);
	[[nodiscard]] bool _readRegister(std::uint8_t reg, std::uint8_t& value);
	[[nodiscard]] bool _configureController();
	[[nodiscard]] std::uint8_t _availableEvents();
	[[nodiscard]] bool _readEvent(RawKeyEvent& event);
	void _drainControllerEvents();
	void _resetStateAfterControllerOverflow();
	void _setPhysicalKeyState(std::uint8_t row, std::uint8_t column, bool pressed);
	void _applyDebouncedState(std::uint32_t timestampMs);
	void _clearPendingKey(std::uint8_t row, std::uint8_t column);
	void _setKeyState(std::uint8_t row, std::uint8_t column, bool pressed, std::uint32_t timestampMs);
	void _updatePressedKeyCache();
	void _pushEvent(InputEventType type, std::uint8_t row, std::uint8_t column, std::uint32_t timestampMs);
	[[nodiscard]] bool _popEvent(InputEvent& event);
	[[nodiscard]] std::uint8_t _modifiersForState(std::uint64_t state) const;
	[[nodiscard]] static std::uint8_t _scanCode(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static std::uint64_t _keyBit(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static bool _isPressed(std::uint64_t state, std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static KeyDefinition const& _keyAt(std::uint8_t row, std::uint8_t column);
	[[nodiscard]] static std::uint32_t _keyIndex(KeyCode key);

	I2cBus& _bus;
	std::uint8_t _address{0x34};
	bool _ready{false};
	std::uint64_t _physicalState{0};
	std::uint64_t _stableState{0};
	std::uint64_t _pendingMask{0};
	std::uint64_t _pendingTargetState{0};
	std::array<std::uint32_t, keyCount> _pendingSinceMs{};
	std::uint8_t _stableModifiers{0};
	std::array<bool, 128> _pressedKeys{};
	std::array<InputEvent, eventQueueCapacity> _events{};
	std::size_t _eventHead{0};
	std::size_t _eventTail{0};
	std::size_t _eventCount{0};
};

} // namespace lexxos::devices
