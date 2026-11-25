#pragma once

// CPU0-only cooperative event. Signal/wait are called from task context,
// never from IRQ. The condition guarded by an event must be checked by its
// owner before parking; the event itself does not queue payloads.
namespace lexxos::platform::rk3506 {

struct CooperativeEvent {};

void waitCooperativeEvent(CooperativeEvent& event);
void signalCooperativeEvent(CooperativeEvent& event);

} // namespace lexxos::platform::rk3506
