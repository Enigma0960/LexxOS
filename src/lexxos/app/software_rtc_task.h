#pragma once

namespace lexxos::app {

struct RuntimeTaskContext;

// Returns success when software RTC is not configured; otherwise starts its worker.
[[nodiscard]] bool startSoftwareRtcTask(RuntimeTaskContext& context);

} // namespace lexxos::app
