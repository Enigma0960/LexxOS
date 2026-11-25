#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

namespace {

	struct HostMutex {
		std::timed_mutex mutex;
	};

} // namespace

Mutex::Mutex() noexcept
    : _state(new (std::nothrow) HostMutex{}) {
}

Mutex::~Mutex() {
	delete static_cast<HostMutex*>(_state);
}

bool Mutex::valid() const noexcept {
	return _state != nullptr;
}

bool Mutex::lock(std::uint32_t timeoutMs) noexcept {
	auto* const mutex = static_cast<HostMutex*>(_state);
	if (!mutex) {
		return false;
	}

	if (timeoutMs == waitForever) {
		mutex->mutex.lock();
		return true;
	}

	return mutex->mutex.try_lock_for(std::chrono::milliseconds(timeoutMs));
}

void Mutex::unlock() noexcept {
	if (_state) {
		static_cast<HostMutex*>(_state)->mutex.unlock();
	}
}

} // namespace lexxos::kernel
