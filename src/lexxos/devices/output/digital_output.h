#pragma once

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

enum class DigitalOutputState : std::uint8_t {
	Unknown,
	Low,
	High
};

// Один владелец GPIO. Вызывайте методы последовательно из одной задачи.
class DigitalOutput {
public:
	explicit DigitalOutput(platform::PlatformGpioService const& gpio);
	DigitalOutput(DigitalOutput const&) = delete;
	DigitalOutput& operator=(DigitalOutput const&) = delete;

	[[nodiscard]] bool initialize(platform::GpioPin pin, bool initialHigh = false);

	// Уровень, подтверждённый GPIO-драйвером. Ошибка записи переводит его в Unknown.
	[[nodiscard]] DigitalOutputState state() const { return _state; }

	[[nodiscard]] platform::GpioPin pin() const { return _pin; }

	[[nodiscard]] bool write(bool high);

	[[nodiscard]] bool setHigh() { return write(true); }

	[[nodiscard]] bool setLow() { return write(false); }

	[[nodiscard]] bool toggle();

private:
	platform::PlatformGpioService const& _gpio;
	platform::GpioPin _pin{};
	DigitalOutputState _state{DigitalOutputState::Unknown};
};

} // namespace lexxos::devices
