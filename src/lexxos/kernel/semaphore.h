#pragma once

#include <cstdint>

#include <lexxos/kernel/synchronization_types.h>

namespace lexxos::kernel {

class Semaphore final {
public:
	Semaphore(std::uint32_t maximum, std::uint32_t initial = 0) noexcept;
	~Semaphore();
	Semaphore(Semaphore const&) = delete;
	Semaphore& operator=(Semaphore const&) = delete;

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool acquire(std::uint32_t timeoutMs = waitForever) noexcept;
	[[nodiscard]] bool release() noexcept;

private:
	void* _state{nullptr};
};

} // namespace lexxos::kernel
