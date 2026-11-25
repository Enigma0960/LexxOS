#pragma once

#include <atomic>
#include <cstdint>

#include <lexxos/devices/bus/detail/bus_transaction_queue_guard.h>
#include <lexxos/devices/device_resource.h>

namespace lexxos::kernel {

class Event;

} // namespace lexxos::kernel

namespace lexxos::devices {

// Одна очередь на физический контроллер; объекты устройств могут разделять её.
// FIFO within a priority. The current transaction is never preempted.
class BusTransactionQueue {
public:
	using WaitFunction = void (*)(std::uint32_t milliseconds);

	BusTransactionQueue() = default;
	BusTransactionQueue(BusTransactionQueue const&) = delete;
	BusTransactionQueue& operator=(BusTransactionQueue const&) = delete;

	void setWaitFunction(WaitFunction wait);

	[[nodiscard]] bool acquire(DeviceResourcePriority priority, std::uint32_t timeoutMs);

	void release();

	[[nodiscard]] bool busy() const;

	[[nodiscard]] std::uint32_t pending() const;

private:
	[[nodiscard]] WaitFunction loadWaitFunction() const;

	struct Request {
		Request* next{nullptr};
		DeviceResourcePriority priority{DeviceResourcePriority::Normal};
		bool granted{false};
		bool notifying{false};
		lexxos::kernel::Event* event{nullptr};
	};

	using Guard = detail::BusTransactionQueueGuard;
	friend class detail::BusTransactionQueueGuard;

#if defined(LEXXOS_PLATFORM_RK3506)
	mutable bool _guard{false};
	WaitFunction _wait{nullptr};
#else
	mutable std::atomic_flag _guard = ATOMIC_FLAG_INIT;
	std::atomic<WaitFunction> _wait{nullptr};
#endif
	Request* _head{nullptr};
	std::uint32_t _pending{0};
	bool _active{false};
};

} // namespace lexxos::devices
