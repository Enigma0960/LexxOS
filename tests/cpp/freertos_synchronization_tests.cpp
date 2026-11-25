#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <lexxos/devices/bus/bus_transaction_queue.h>
#include <lexxos/kernel/synchronization.h>

namespace {

struct FakeSemaphore {
	std::mutex mutex;
	std::condition_variable ready;
	unsigned count{}, maximum{};
};

std::recursive_mutex criticalSection;

void check(bool ok, char const* text) {
	if (!ok) {
		std::cerr << text << '\n';
		std::abort();
	}
}

void sleepMs(unsigned value) {
	std::this_thread::sleep_for(std::chrono::milliseconds(value));
}

void waitPending(lexxos::devices::BusTransactionQueue& queue, unsigned count) {
	for (unsigned i = 0; i < 500 && queue.pending() != count; ++i) {
		sleepMs(1);
	}
	check(queue.pending() == count, "waiter was not enqueued");
}

FakeSemaphore* makeSemaphore(unsigned maximum, unsigned initial) {
	return new FakeSemaphore{{}, {}, initial, maximum};
}

} // namespace

void testEnterCritical() {
	criticalSection.lock();
}

void testExitCritical() {
	criticalSection.unlock();
}

int xTaskGetSchedulerState() {
	return taskSCHEDULER_RUNNING;
}

void vTaskDelay(TickType_t ticks) {
	sleepMs(ticks);
}

SemaphoreHandle_t xSemaphoreCreateBinary() {
	return makeSemaphore(1, 0);
}

SemaphoreHandle_t xSemaphoreCreateMutex() {
	return makeSemaphore(1, 1);
}

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t initial) {
	return makeSemaphore(max, initial);
}

void vSemaphoreDelete(SemaphoreHandle_t handle) {
	delete static_cast<FakeSemaphore*>(handle);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t ticks) {
	auto* sem = static_cast<FakeSemaphore*>(handle);
	std::unique_lock lock{sem->mutex};
	if (ticks == portMAX_DELAY) {
		sem->ready.wait(lock, [sem] { return sem->count != 0; });
	} else if (!sem->ready.wait_for(lock, std::chrono::milliseconds(ticks),
	               [sem] { return sem->count != 0; })) {
		return pdFALSE;
	}
	--sem->count;
	return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
	auto* sem = static_cast<FakeSemaphore*>(handle);
	{
		std::lock_guard lock{sem->mutex};
		if (sem->count >= sem->maximum) {
			return pdFALSE;
		}

		++sem->count;
	}
	sem->ready.notify_one();
	return pdTRUE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t handle, BaseType_t* higherPriorityWoken) {
	*static_cast<BaseType_t*>(higherPriorityWoken) = pdTRUE;
	return xSemaphoreGive(handle);
}

int main() {
	using lexxos::devices::BusTransactionQueue;
	using lexxos::devices::DeviceResourcePriority;
	lexxos::kernel::Event event;
	check(event.valid() && !event.wait(0), "initial event status");
	check(event.signal() && !event.signal() && event.wait(0) && !event.wait(0), "binary event behavior");
	bool woken{};
	check(event.signalFromIsr(woken) && woken && event.wait(0), "ISR event notification");
	lexxos::kernel::Mutex mutex;
	check(mutex.valid() && mutex.lock(0), "mutex acquire");
	mutex.unlock();
	lexxos::kernel::Semaphore semaphore{2, 1};
	check(semaphore.valid() && semaphore.acquire(0) && semaphore.release(), "counting semaphore");

	BusTransactionQueue queue;
	check(queue.acquire(DeviceResourcePriority::Normal, 0), "first owner");
	std::vector<int> order;
	std::thread background([&] {
		check(queue.acquire(DeviceResourcePriority::Background, 500), "background waiter");
		order.push_back(2);
		queue.release();
	});

	waitPending(queue, 1);
	std::thread system([&] {
		check(queue.acquire(DeviceResourcePriority::System, 500), "system waiter");
		order.push_back(1);
		queue.release();
	});
	waitPending(queue, 2);

	std::thread secondSystem([&] {
		check(queue.acquire(DeviceResourcePriority::System, 500), "second system waiter");
		order.push_back(3);
		queue.release();
	});
	waitPending(queue, 3);

	queue.release();
	background.join();
	system.join();

	secondSystem.join();
	check(order == std::vector<int>{1, 3, 2} && !queue.busy() && queue.pending() == 0,
	    "priority order and equal-priority FIFO");

	check(queue.acquire(DeviceResourcePriority::Normal, 0), "reacquire");

	bool survivorAcquired = false;
	std::thread survivor([&] {
		survivorAcquired = queue.acquire(DeviceResourcePriority::Normal, 500);
		if (survivorAcquired) {
			queue.release();
		}
	});

	waitPending(queue, 1);
	std::thread timeout([&] { check(!queue.acquire(DeviceResourcePriority::Critical, 100), "timeout expected"); });
	waitPending(queue, 2);

	timeout.join();
	check(queue.pending() == 1 && queue.busy(), "timed-out high-priority waiter escaped");
	queue.release();

	survivor.join();
	check(survivorAcquired && !queue.busy() && queue.pending() == 0,
	    "lower-priority survivor did not receive bus");

	// Exercise both sides of the timeout/release boundary without assuming
	// which task wins a close scheduling race.
	constexpr std::array<unsigned, 6> releaseDelaysMs{0, 20, 35, 40, 45, 60};
	unsigned granted = 0;
	unsigned timedOut = 0;
	for (unsigned round = 0; round < 18; ++round) {
		check(queue.acquire(DeviceResourcePriority::Normal, 0), "race initial owner");
		bool acquired = false;
		std::thread waiter([&] {
			acquired = queue.acquire(DeviceResourcePriority::System, 40);
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
			++timedOut;
		}
	}

	check(granted != 0 && timedOut != 0, "timeout/release boundary not covered");
}
