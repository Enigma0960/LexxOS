#pragma once

namespace lexxos::kernel::espidf::detail {

// Только task context; не использовать в ISR или при удержании portMUX.
class TaskRegistryGuard final {
public:
	TaskRegistryGuard();
	~TaskRegistryGuard();
	TaskRegistryGuard(TaskRegistryGuard const&) = delete;
	TaskRegistryGuard& operator=(TaskRegistryGuard const&) = delete;
};

} // namespace lexxos::kernel::espidf::detail
