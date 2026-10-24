#pragma once

#include <lexxos/kernel/scheduler.h>

namespace lexxos::kernel::pico::detail {

struct BootstrapTask {
	TaskEntry entry{nullptr};
	void* userData{nullptr};
	TaskSnapshot snapshot{};
};

} // namespace lexxos::kernel::pico::detail
