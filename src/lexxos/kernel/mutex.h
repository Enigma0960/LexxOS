#pragma once

#include <cstdint>

#include <lexxos/kernel/synchronization_types.h>

namespace lexxos::kernel {

class Mutex final {
public:
	Mutex() noexcept;
	~Mutex();
	Mutex(Mutex const&) = delete;
	Mutex& operator=(Mutex const&) = delete;

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] bool lock(std::uint32_t timeoutMs = waitForever) noexcept;
	void unlock() noexcept;

private:
	void* _state{nullptr};
};

} // namespace lexxos::kernel
