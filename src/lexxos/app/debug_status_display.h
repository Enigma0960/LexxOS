#pragma once

namespace lexxos::os {
class OperatingSystem;
} // namespace lexxos::os

namespace lexxos::app {

class DebugStatusDisplay {
public:
	explicit DebugStatusDisplay(lexxos::os::OperatingSystem& operatingSystem);

	[[nodiscard]] bool initialize();

	void drawStatic();
	void showBooting();
	void refresh();
	void showTaskFailure();

private:
	lexxos::os::OperatingSystem& _operatingSystem;
};

} // namespace lexxos::app
