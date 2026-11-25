#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#include <lexxos/kernel/synchronization.h>

namespace {

} // namespace

namespace lexxos::kernel {

void synchronizationBackoff() noexcept {
	std::this_thread::yield();
}

} // namespace lexxos::kernel
