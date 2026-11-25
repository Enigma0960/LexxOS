#include <lexxos/core/core_context.h>

#include <lexxos/core/core_system.h>

namespace lexxos::core {

CoreContext::CoreContext(CoreSystem& system)
    : _system(system) {
}

lexxos::devices::DeviceManager& CoreContext::devices() const {
	return _system.devices();
}

lexxos::platform::PlatformServices const& CoreContext::platform() const {
	return _system.platform();
}

lexxos::lifecycle::EventBus& CoreContext::events() const {
	return _system.events();
}

lexxos::runtime::CapabilitySet CoreContext::capabilities() const {
	return _system.capabilities();
}

} // namespace lexxos::core
