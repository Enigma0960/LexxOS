#include <lexxos/devices/rk3506/picosd_media_hooks.h>

namespace lexxos::platform::rk3506 {

bool PicoSdMediaHooks::mounted() const {
	return context == nullptr || isMounted == nullptr || suppressAutoProbe == nullptr || isMounted(context);
}

void PicoSdMediaHooks::suppress() const {
	if (context != nullptr && suppressAutoProbe != nullptr) {
		suppressAutoProbe(context);
	}
}

} // namespace lexxos::platform::rk3506
