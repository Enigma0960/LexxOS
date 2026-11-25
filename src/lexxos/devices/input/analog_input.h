#pragma once

#include <cstdint>

namespace lexxos::devices {

// Вызовы из контекста задачи. При ошибке выходной аргумент сохраняется.
class AnalogInput {
public:
	virtual ~AnalogInput() = default;
	[[nodiscard]] virtual bool isReady() const = 0;
	[[nodiscard]] virtual std::uint32_t maxCode() const = 0;
	virtual bool readRaw(std::uint32_t& code) = 0;
	[[nodiscard]] virtual bool hasCalibration() const = 0;
	// Отдельное измерение. false при отсутствии калибровки или ошибке чтения.
	virtual bool readMillivolts(std::uint32_t& millivolts) = 0;
};

} // namespace lexxos::devices
