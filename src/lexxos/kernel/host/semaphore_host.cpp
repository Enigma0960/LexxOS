#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

namespace {

	struct HostSemaphore {
		std::mutex mutex;
		std::condition_variable ready;
		std::uint32_t count;
		std::uint32_t maximum;
	};

} // namespace

Semaphore::Semaphore(std::uint32_t maximum, std::uint32_t initial) noexcept {
	if (maximum != 0U && initial <= maximum) {
		_state = new (std::nothrow) HostSemaphore{{}, {}, initial, maximum};
	}
}

Semaphore::~Semaphore() {
	delete static_cast<HostSemaphore*>(_state);
}

bool Semaphore::valid() const noexcept {
	return _state != nullptr;
}

bool Semaphore::acquire(std::uint32_t timeoutMs) noexcept {
	auto* const sem = static_cast<HostSemaphore*>(_state);
	if (!sem) {
		return false;
	}

	std::unique_lock lock{sem->mutex};
	if (timeoutMs == waitForever) {
		sem->ready.wait(lock, [sem] { return sem->count != 0; });
	} else if (!sem->ready.wait_for(lock, std::chrono::milliseconds(timeoutMs),
	               [sem] { return sem->count != 0; })) {
		return false;
	}
	--sem->count;
	return true;
}

bool Semaphore::release() noexcept {
	auto* const sem = static_cast<HostSemaphore*>(_state);
	if (!sem) {
		return false;
	}

	{
		std::lock_guard lock{sem->mutex};
		if (sem->count == sem->maximum) {
			return false;
		}

		++sem->count;
	}
	sem->ready.notify_one();
	return true;
}

} // namespace lexxos::kernel
