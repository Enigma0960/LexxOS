#include <cstdlib>
#include <iostream>

#include <lexxos/kernel/freertos/lazy_mutex.h>

namespace {

unsigned critical{}, allocations{}, deletions{};
int candidate{}, winner{};
bool fail{};
void** competingStorage{};

void expect(bool ok) {
	if (!ok) {
		std::cerr << "lazy mutex contract failed\n";
		std::exit(1);
	}
}

} // namespace

void testEnterCritical() {
	++critical;
}

void testExitCritical() {
	expect(critical == 1);
	--critical;
}

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
	expect(critical == 0);
	++allocations;
	// Model another task completing publication while allocation was preempted.
	if (competingStorage) {
		*competingStorage = &winner;
	}

	return fail ? nullptr : &candidate;
}

void vSemaphoreDelete(SemaphoreHandle_t mutex) {
	expect(critical == 0 && mutex == &candidate);
	++deletions;
}

int main() {
	void* storage{};
	fail = true;
	expect(lexxos::kernel::freertos::ensureRecursiveMutex(storage) == nullptr);
	expect(storage == nullptr && deletions == 0);
	fail = false;
	expect(lexxos::kernel::freertos::ensureRecursiveMutex(storage) == &candidate);
	expect(lexxos::kernel::freertos::ensureRecursiveMutex(storage) == &candidate);
	expect(allocations == 2 && deletions == 0);
	storage = nullptr;
	competingStorage = &storage;
	expect(lexxos::kernel::freertos::ensureRecursiveMutex(storage) == &winner);
	expect(storage == &winner && deletions == 1 && critical == 0);
	storage = nullptr;
	fail = true;
	expect(lexxos::kernel::freertos::ensureRecursiveMutex(storage) == &winner);
	expect(deletions == 1 && critical == 0);
}
