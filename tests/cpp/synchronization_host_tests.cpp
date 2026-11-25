#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include <lexxos/devices/bus/bus_transaction_queue.h>
#include <lexxos/kernel/synchronization.h>

namespace {

void check(bool value, char const* description) {
	if (!value) {
		std::cerr << description << '\n';
		std::abort();
	}
}

void sleepMs(std::uint32_t ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void waitPending(lexxos::devices::BusTransactionQueue& queue, std::uint32_t expected) {
	for (unsigned i = 0; i < 300 && queue.pending() != expected; ++i) {
		sleepMs(1);
	}
	check(queue.pending() == expected, "waiter not queued");
}

} // namespace

int main() {
	namespace kernel = lexxos::kernel;
	namespace devices = lexxos::devices;
	kernel::Event event;
	check(event.valid() && !event.wait(0), "event initial state");
	check(event.signal() && event.wait(0) && !event.wait(0), "event must auto reset");
	check(event.signal() && !event.signal() && event.wait(0) && !event.wait(0), "event is binary");
	bool wakeFromIsr{};
	check(!event.signalFromIsr(wakeFromIsr) && !wakeFromIsr, "host ISR must be rejected");
	std::thread signaller([&] { sleepMs(10); check(event.signal(), "signal failed"); });
	check(event.wait(500), "event wait missed notification");
	signaller.join();
	check(!event.wait(3), "event timeout not observed");

	kernel::Mutex mutex;
	check(mutex.valid() && mutex.lock(0), "mutex initial lock");
	std::atomic<bool> timedOut{false};
	std::thread contending([&] { timedOut.store(!mutex.lock(10)); });
	contending.join();
	check(timedOut.load(), "mutex did not exclude another thread");
	mutex.unlock();
	check(mutex.lock(20), "mutex failed after unlock");
	mutex.unlock();

	kernel::Semaphore sem{2, 1};
	check(sem.valid() && sem.acquire(0) && !sem.acquire(0), "semaphore initial count");
	check(sem.release() && sem.release() && !sem.release(), "semaphore maximum count");
	check(sem.acquire(0) && sem.acquire(0) && !sem.acquire(0), "semaphore underflow");
	check(!kernel::Semaphore{0}.valid() && !kernel::Semaphore{1, 2}.valid(), "semaphore bounds");

	devices::BusTransactionQueue queue;
	queue.setWaitFunction(sleepMs);
	check(queue.acquire(devices::DeviceResourcePriority::Normal, 0), "initial bus acquisition");
	std::vector<int> order;
	std::thread background([&] {
		check(queue.acquire(devices::DeviceResourcePriority::Background, 500), "background missed wake");
		order.push_back(2);
		queue.release();
	});

	waitPending(queue, 1);
	std::thread system([&] {
		check(queue.acquire(devices::DeviceResourcePriority::System, 500), "system missed wake");
		order.push_back(1);
		queue.release();
	});

	waitPending(queue, 2);
	std::thread secondSystem([&] {
		check(queue.acquire(devices::DeviceResourcePriority::System, 500), "second system missed wake");
		order.push_back(3);
		queue.release();
	});

	waitPending(queue, 3);
	queue.release();
	background.join();
	system.join();

	secondSystem.join();
	check(order == std::vector<int>{1, 3, 2} && !queue.busy() && queue.pending() == 0,
	    "priority order or equal-priority FIFO");
	check(queue.acquire(devices::DeviceResourcePriority::Normal, 0), "bus reacquisition");
	bool survivorAcquired = false;
	std::thread survivor([&] {
		survivorAcquired = queue.acquire(devices::DeviceResourcePriority::Normal, 500);
		if (survivorAcquired) {
			queue.release();
		}
	});

	waitPending(queue, 1);
	std::thread timeout([&] { check(!queue.acquire(devices::DeviceResourcePriority::Critical, 100), "timeout expected"); });
	waitPending(queue, 2);

	timeout.join();
	check(queue.pending() == 1 && queue.busy(), "timeout did not unlink high-priority waiter");
	queue.release();

	survivor.join();
	check(survivorAcquired && !queue.busy() && queue.pending() == 0,
	    "timeout removed a lower-priority survivor");

	// Exercise both sides of the timeout/release boundary without assuming
	// which task wins a close scheduling race.
	constexpr std::array<unsigned, 6> releaseDelaysMs{0, 20, 35, 40, 45, 60};
	unsigned granted = 0;
	unsigned timeoutCount = 0;
	for (unsigned round = 0; round < 18; ++round) {
		check(queue.acquire(devices::DeviceResourcePriority::Normal, 0), "race initial owner");
		bool acquired = false;
		std::thread waiter([&] {
			acquired = queue.acquire(devices::DeviceResourcePriority::System, 40);
			if (acquired) {
				queue.release();
			}
		});

		waitPending(queue, 1);
		sleepMs(releaseDelaysMs[round % releaseDelaysMs.size()]);
		queue.release();
		waiter.join();

		check(queue.pending() == 0 && !queue.busy(), "race left a stale waiter or owner");
		if (acquired) {
			++granted;
		} else {
			++timeoutCount;
		}
	}

	check(granted != 0 && timeoutCount != 0, "timeout/release boundary not covered");
}
