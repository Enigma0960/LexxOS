#pragma once
#include <cstdint>

namespace lexxos::devices {

// Один владелец канала. Вызывайте методы последовательно из одной задачи.
class PwmOutput {
public:
	virtual ~PwmOutput() = default;
	[[nodiscard]] virtual bool isReady() const = 0;
	[[nodiscard]] virtual std::uint32_t frequencyHz() const = 0;
	// 0 — постоянно неактивный выход, 1000 — постоянно активный.
	// Значение вне диапазона отклоняется до обращения к оборудованию.
	virtual bool setDutyPermille(std::uint16_t duty) = 0;
	// Последнее подтверждённое задание, с учётом возможной ошибки драйвера.
	// При false выходной аргумент сохраняется.
	[[nodiscard]] virtual bool dutyPermille(std::uint16_t& duty) const = 0;
};

} // namespace lexxos::devices
