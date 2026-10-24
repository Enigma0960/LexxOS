#pragma once

#include <string_view>

namespace lexxos::os {
class OperatingSystem;
} // namespace lexxos::os

namespace lexxos::app {

using SdPostMountHandler = void (*)(lexxos::os::OperatingSystem& operatingSystem, std::string_view reason);

class SdAutomountService {
public:
	static constexpr std::string_view blockDevicePath{"/dev/sd0"};
	static constexpr std::string_view mountPath{"/mnt/sd"};

	explicit SdAutomountService(lexxos::os::OperatingSystem& operatingSystem);

	void setPostMountHandler(SdPostMountHandler handler);
	bool mount(std::string_view reason);
	void unmount(std::string_view reason);

	[[nodiscard]] bool isMounted() const;

private:
	lexxos::os::OperatingSystem& _operatingSystem;
	SdPostMountHandler _postMountHandler{nullptr};
};

} // namespace lexxos::app
