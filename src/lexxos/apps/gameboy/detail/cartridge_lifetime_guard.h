#pragma once

namespace lexxos::gameboy {

class Bus;
class Cartridge;

} // namespace lexxos::gameboy

namespace lexxos::apps::gameboy_programs::detail {

class CartridgeLifetimeGuard final {
public:
	explicit CartridgeLifetimeGuard(lexxos::gameboy::Cartridge& cartridge);
	CartridgeLifetimeGuard(CartridgeLifetimeGuard const&) = delete;
	CartridgeLifetimeGuard& operator=(CartridgeLifetimeGuard const&) = delete;
	~CartridgeLifetimeGuard();

	void attachBus(lexxos::gameboy::Bus& bus);

private:
	lexxos::gameboy::Cartridge* _cartridge{nullptr};
	lexxos::gameboy::Bus* _bus{nullptr};
};

} // namespace lexxos::apps::gameboy_programs::detail
