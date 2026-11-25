#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace lexxos::kernel {

// true — вызвать обработчик снова, false — завершить его.
using CooperativeEntry = bool (*)(void*);

class CooperativeLoop final {
public:
	static constexpr std::size_t capacity = 8;
	// Первый вызов после periodMs. Нулевой период — один вызов на poll().
	// Регистрация и poll() выполняются из одного контекста, вне обработчиков.
	bool add(CooperativeEntry entry, void* context, std::uint32_t periodMs, std::uint32_t nowMs);
	void poll(std::uint32_t nowMs);
	[[nodiscard]] std::size_t taskCount() const;

private:
	struct Task {
		CooperativeEntry entry{};
		void* context{};
		std::uint32_t periodMs{};
		std::uint32_t deadline{};
	};

	std::array<Task, capacity> _tasks{};
	bool _polling{};
};

} // namespace lexxos::kernel
