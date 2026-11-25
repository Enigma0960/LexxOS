#include <atomic>
#include <new>

#include <lexxos/kernel/cooperative/detail/synchronization_support.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/synchronization.h>

namespace lexxos::kernel::detail {

static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

bool acquire(void* state) {
	if (!state) {
		return false;
	}

	auto& value = static_cast<Counter*>(state)->value;
	auto current = value.load(std::memory_order_acquire);
	while (current) {
		if (value.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel)) {
			return true;
		}
	}

	return false;
}

bool release(void* state) {
	if (!state) {
		return false;
	}

	auto& counter = *static_cast<Counter*>(state);
	auto current = counter.value.load(std::memory_order_acquire);
	while (current < counter.maximum) {
		if (counter.value.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel)) {
			return true;
		}
	}

	return false;
}

} // namespace lexxos::kernel::detail

namespace lexxos::kernel {

// В однопоточном цикле ожидание другого обработчика привело бы к deadlock.
// Положительный timeout не блокирует цикл, вызывающий повторяет попытку на следующем шаге.
void synchronizationBackoff() noexcept {
	schedulerBackend().yield();
}

} // namespace lexxos::kernel
