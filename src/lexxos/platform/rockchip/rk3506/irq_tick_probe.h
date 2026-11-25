#pragma once

#include <cstdint>

namespace lexxos::kernel {

class Event;

} // namespace lexxos::kernel

namespace lexxos::platform::rk3506 {

// Diagnostic hook for the FreeRTOS-owned virtual timer PPI27 only.
// Arm/disarm from task context; disarm before the Event is destroyed.
// Does not subscribe to or reconfigure a GIC interrupt line.
[[nodiscard]] bool armTickEventProbe(kernel::Event& event) noexcept;
[[nodiscard]] std::uint32_t disarmTickEventProbe(kernel::Event& event) noexcept;

} // namespace lexxos::platform::rk3506
