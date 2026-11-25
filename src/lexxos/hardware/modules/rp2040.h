#pragma once

#include <cstdint>

namespace lexxos::hardware::modules::rp2040 {

struct Pin {
	int number{-1};

	[[nodiscard]] constexpr bool isValid() const {
		return number >= 0 && number < 30;
	}

	constexpr bool operator==(Pin const&) const = default;
};

struct UartId {
	int index{-1};
};

struct I2cId {
	int index{-1};
};

struct SpiId {
	int index{-1};
};

struct UartRoute {
	UartId controller;
	Pin tx;
	Pin rx;
};

struct I2cRoute {
	I2cId controller;
	Pin sda;
	Pin scl;
};

struct SpiRoute {
	SpiId controller;
	Pin clock;
	Pin mosi;
	Pin miso;
};

// Декларации описывают маршруты конкретного продукта, не весь pinmux RP2040.
[[nodiscard]] constexpr bool valid(UartRoute const& route) {
	return route.controller.index >= 0 && route.controller.index < 2 && route.tx.isValid() && route.rx.isValid() && route.tx != route.rx;
}

[[nodiscard]] constexpr bool valid(I2cRoute const& route) {
	return route.controller.index >= 0 && route.controller.index < 2 && route.sda.isValid() && route.scl.isValid() && route.sda != route.scl;
}

[[nodiscard]] constexpr bool valid(SpiRoute const& route) {
	return route.controller.index >= 0 && route.controller.index < 2 && route.clock.isValid() && route.mosi.isValid() && route.miso.isValid() && route.clock != route.mosi && route.clock != route.miso && route.mosi != route.miso;
}

} // namespace lexxos::hardware::modules::rp2040
