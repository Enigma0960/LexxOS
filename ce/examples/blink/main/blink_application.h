#pragma once

#include <lexxos/devices/output/digital_output.h>

namespace lexxos::core {

class CoreSystem;

} // namespace lexxos::core

namespace lexxos::ce::blink {

class BlinkApplication final {
public:
	explicit BlinkApplication(lexxos::core::CoreSystem& system);
	[[nodiscard]] bool initialize();
	// Ждёт 1000 мс, переключает LED и сообщает успешно установленное состояние.
	[[nodiscard]] bool step();
	// Один шаг без ожидания, для кооперативного цикла.
	[[nodiscard]] bool tick();

private:
	void reportState() const;
	lexxos::core::CoreSystem& _system;
	lexxos::devices::DigitalOutput* _led{};
	bool _activeHigh{true};
	bool _ready{false};
};

} // namespace lexxos::ce::blink
