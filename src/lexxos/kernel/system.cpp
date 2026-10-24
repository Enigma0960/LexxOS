#include <lexxos/kernel/system.h>

#include <atomic>

namespace {

std::atomic_bool g_shutdownRequested{false};

} // namespace

void lexxos::kernel::requestShutdown() {
	g_shutdownRequested.store(true, std::memory_order_release);
}

void lexxos::kernel::resetShutdownRequest() {
	g_shutdownRequested.store(false, std::memory_order_release);
}

bool lexxos::kernel::isShutdownRequested() {
	return g_shutdownRequested.load(std::memory_order_acquire);
}
