#include <atomic>
#include <new>

#include <lexxos/kernel/cooperative/detail/synchronization_support.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

using detail::acquire;
using detail::Counter;
using detail::release;

Semaphore::Semaphore(std::uint32_t maximum, std::uint32_t initial) noexcept {
	if (maximum && initial <= maximum) {
		_state = new (std::nothrow) Counter{{initial}, maximum};
	}
}

Semaphore::~Semaphore() {
	delete static_cast<Counter*>(_state);
}

bool Semaphore::valid() const noexcept {
	return _state != nullptr;
}

bool Semaphore::acquire(std::uint32_t) noexcept {
	return detail::acquire(_state);
}

bool Semaphore::release() noexcept {
	return detail::release(_state);
}

} // namespace lexxos::kernel
