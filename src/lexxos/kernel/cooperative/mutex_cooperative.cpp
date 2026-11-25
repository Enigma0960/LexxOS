#include <atomic>
#include <new>

#include <lexxos/kernel/cooperative/detail/synchronization_support.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

using detail::acquire;
using detail::Counter;
using detail::release;

Mutex::Mutex() noexcept
    : _state(new (std::nothrow) Counter{{1}, 1}) {
}

Mutex::~Mutex() {
	delete static_cast<Counter*>(_state);
}

bool Mutex::valid() const noexcept {
	return _state != nullptr;
}

bool Mutex::lock(std::uint32_t) noexcept {
	return acquire(_state);
}

void Mutex::unlock() noexcept {
	(void) release(_state);
}

} // namespace lexxos::kernel
