#pragma once

#include <thread>

#include <lexxos/kernel/scheduler.h>

namespace lexxos::kernel::host::detail {

struct HostTask {
	TaskEntry entry{nullptr};
	void* userData{nullptr};
	TaskSnapshot snapshot{};
	std::thread thread;
	bool allocated{false};
};

} // namespace lexxos::kernel::host::detail
