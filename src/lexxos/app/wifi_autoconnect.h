#pragma once

namespace lexxos::os {
class OperatingSystem;
}

namespace lexxos::app {

struct RuntimeTaskContext;

[[nodiscard]] bool triggerWifiAutoconnect(
    lexxos::os::OperatingSystem& operatingSystem,
    bool initialDelay,
    bool retryCredentials);
// Task creation failure is reported to the console and does not abort boot.
void startWifiAutoconnectTask(RuntimeTaskContext& context);

} // namespace lexxos::app
