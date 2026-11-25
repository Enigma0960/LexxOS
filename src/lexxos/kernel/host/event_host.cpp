#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel {

namespace {

	struct HostEvent {
		std::mutex mutex;
		std::condition_variable ready;
		bool signalled{};
	};

} // namespace

Event::Event() noexcept
    : _state(new (std::nothrow) HostEvent{}) {
}

Event::~Event() {
	delete static_cast<HostEvent*>(_state);
}

bool Event::valid() const noexcept {
	return _state != nullptr;
}

bool Event::wait(std::uint32_t timeoutMs) noexcept {
	auto* const event = static_cast<HostEvent*>(_state);
	if (!event) {
		return false;
	}

	std::unique_lock lock{event->mutex};
	if (timeoutMs == waitForever) {
		event->ready.wait(lock, [event] { return event->signalled; });
	} else if (!event->ready.wait_for(lock, std::chrono::milliseconds(timeoutMs),
	               [event] { return event->signalled; })) {
		return false;
	}
	event->signalled = false;
	return true;
}

bool Event::signal() noexcept {
	auto* const event = static_cast<HostEvent*>(_state);
	if (!event) {
		return false;
	}

	{
		std::lock_guard lock{event->mutex};
		if (event->signalled) {
			return false;
		}

		event->signalled = true;
	}
	event->ready.notify_one();
	return true;
}

bool Event::signalFromIsr(bool&) noexcept {
	return false;
}

} // namespace lexxos::kernel
