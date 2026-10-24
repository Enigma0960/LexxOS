#pragma once

namespace lexxos::kernel {

void requestShutdown();
void resetShutdownRequest();
[[nodiscard]] bool isShutdownRequested();

} // namespace lexxos::kernel
