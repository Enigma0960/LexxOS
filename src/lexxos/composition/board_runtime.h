#pragma once

namespace lexxos::os {

class OperatingSystem;

} // namespace lexxos::os

namespace lexxos::composition {

void registerBoardRuntimePrograms(lexxos::os::OperatingSystem& operatingSystem);
bool startBoardRuntimeTasks(lexxos::os::OperatingSystem& operatingSystem);

} // namespace lexxos::composition
