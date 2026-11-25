#include <lexxos/composition/detail/platform_device_providers.h>

namespace lexxos::composition::detail {

std::span<BoardDeviceProvider const> selectedBoardDeviceProviders() {
	static constexpr BoardDeviceProvider providers[]{registerLegacyBoardDevices};
	return providers;
}

} // namespace lexxos::composition::detail
