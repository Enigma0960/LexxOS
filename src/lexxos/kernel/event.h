#pragma once

#include <cstdint>

#include <lexxos/kernel/synchronization_types.h>

namespace lexxos::kernel {

class Event final {
public:
	Event() noexcept;
	~Event();
	Event(Event const&) = delete;
	Event& operator=(Event const&) = delete;

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool wait(std::uint32_t timeoutMs = waitForever) noexcept;
	[[nodiscard]] bool signal() noexcept;

	// ISR use is supported only by RTOS backends and only at an API-safe IRQ
	// priority. The caller performs the platform-specific yield if requested.
	[[nodiscard]] bool signalFromIsr(bool& higherPriorityTaskWoken) noexcept;

private:
	void* _state{nullptr};
};

} // namespace lexxos::kernel
