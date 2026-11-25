#include <lexxos/kernel/cooperative/cooperative_loop.h>

#include <limits>

namespace lexxos::kernel {

bool CooperativeLoop::add(CooperativeEntry entry, void* context, std::uint32_t periodMs, std::uint32_t nowMs) {
	if (!entry || _polling || periodMs > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
		return false;
	}

	for (auto& task : _tasks) {
		if (!task.entry) {
			task = {entry, context, periodMs, nowMs + periodMs};
			return true;
		}
	}

	return false;
}

void CooperativeLoop::poll(std::uint32_t nowMs) {
	if (_polling) {
		return;
	}

	_polling = true;
	for (auto& task : _tasks) {
		if (!task.entry || static_cast<std::int32_t>(nowMs - task.deadline) < 0) {
			continue;
		}

		if (task.entry(task.context)) {
			task.deadline = nowMs + task.periodMs;
		} else {
			task = {};
		}
	}

	_polling = false;
}

std::size_t CooperativeLoop::taskCount() const {
	std::size_t count = 0;
	for (auto const& task : _tasks) {
		if (task.entry) {
			++count;
		}
	}

	return count;
}

} // namespace lexxos::kernel
