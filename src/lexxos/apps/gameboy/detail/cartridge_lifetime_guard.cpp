#include <lexxos/apps/gameboy/detail/cartridge_lifetime_guard.h>

#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cartridge.h>

namespace lexxos::apps::gameboy_programs::detail {

CartridgeLifetimeGuard::CartridgeLifetimeGuard(lexxos::gameboy::Cartridge& cartridge)
    : _cartridge(&cartridge) {
}

CartridgeLifetimeGuard::~CartridgeLifetimeGuard() {
	if (_bus != nullptr) {
		_bus->attachCartridge(nullptr);
	}
	if (_cartridge != nullptr) {
		_cartridge->close();
	}
}

void CartridgeLifetimeGuard::attachBus(lexxos::gameboy::Bus& bus) {
	_bus = &bus;
	_bus->attachCartridge(_cartridge);
}

} // namespace lexxos::apps::gameboy_programs::detail
