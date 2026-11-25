#pragma once

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

enum class DigitalInputEvent : std::uint8_t {
	Changed,
	BecameHigh,
	BecameLow,
	SingleClick,
	DoubleClick,
	TripleClick,
	Hold,
	LongHold,
};

struct DigitalInputConfig {
	platform::GpioPin pin{};
	platform::GpioPull pull{platform::GpioPull::None};
	platform::GpioActiveLevel activeLevel{platform::GpioActiveLevel::Low};
	std::uint32_t debounceMs{platform::GpioInputTiming{}.debounceMs};
	std::uint32_t clickGapMs{platform::GpioInputTiming{}.clickGapMs};
	std::uint32_t holdMs{platform::GpioInputTiming{}.holdMs};
	std::uint32_t longHoldMs{platform::GpioInputTiming{}.longHoldMs};
};

// Один экземпляр на вход. Обработчик вызывается синхронно из poll() в задаче приложения.
class DigitalInput {
public:
	using Handler = void (*)(DigitalInputEvent event, bool high, void* context);
	DigitalInput(DigitalInputConfig config, platform::PlatformGpioService const& gpio);

	// Начальный уровень служит точкой отсчёта. Зажатая при запуске кнопка ожидает отпускания.
	[[nodiscard]] bool initialize();
	void setHandler(Handler handler, void* context = nullptr);
	[[nodiscard]] bool initialize(Handler handler, void* context = nullptr);
	// Монотонное время в миллисекундах. При ошибке GPIO повторите initialize().
	[[nodiscard]] bool poll(std::uint32_t nowMs);

	[[nodiscard]] bool high() const { return _high; }

	[[nodiscard]] bool pressed() const;

private:
	void advanceTimers(std::uint32_t nowMs);
	void acceptEdge(std::uint32_t edgeMs);
	void finishClicks();
	void emit(DigitalInputEvent event) const;

	DigitalInputConfig _config;
	platform::PlatformGpioService const& _gpio;
	Handler _handler{};
	void* _context{};
	bool _ready{};
	bool _high{};
	bool _rawHigh{};
	bool _ignoreUntilRelease{};
	bool _held{};
	bool _longHeld{};
	std::uint8_t _clicks{};
	std::uint32_t _rawSince{};
	std::uint32_t _edgeTime{};
};

} // namespace lexxos::devices
