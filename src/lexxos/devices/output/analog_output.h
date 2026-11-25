#pragma once

#include <cstdint>

namespace lexxos::devices {

// Один владелец. Методы вызываются последовательно из задачи, не из ISR.
class AnalogOutput {
public:
	virtual ~AnalogOutput() = default;
	[[nodiscard]] virtual bool isReady() const = 0;
	[[nodiscard]] virtual std::uint32_t maxCode() const = 0;
	// Диапазон 0…maxCode(). Недопустимый код не меняет выход и последнее задание.
	virtual bool writeRaw(std::uint32_t code) = 0;
	// Последнее подтверждённое задание, а не измерение физического напряжения.
	// После аппаратной ошибки возвращает false и сохраняет выходной аргумент.
	[[nodiscard]] virtual bool currentCode(std::uint32_t& code) const = 0;
};

} // namespace lexxos::devices
