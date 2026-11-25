#pragma once

#include <thread>

#include <lexxos/kernel/scheduler.h>

namespace lexxos::emulator::detail {

struct SimTask {
	lexxos::kernel::TaskEntry entry{nullptr};
	void* userData{nullptr};
	lexxos::kernel::TaskSnapshot snapshot{};
	std::thread thread;
};

} // namespace lexxos::emulator::detail
