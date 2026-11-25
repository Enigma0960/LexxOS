#include <atomic>
#include <new>

#include <lexxos/kernel/cooperative/detail/synchronization_support.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

using detail::acquire;
using detail::Counter;
using detail::release;

Event::Event() noexcept
    : _state(new (std::nothrow) Counter{{0}, 1}) {
}

Event::~Event() {
	delete static_cast<Counter*>(_state);
}

bool Event::valid() const noexcept {
	return _state != nullptr;
}

bool Event::wait(std::uint32_t) noexcept {
	return acquire(_state);
}

bool Event::signal() noexcept {
	return release(_state);
}

bool Event::signalFromIsr(bool&) noexcept {
	return release(_state);
}

} // namespace lexxos::kernel
