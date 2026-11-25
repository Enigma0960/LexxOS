#pragma once

#include <array>
#include <optional>
#include <span>

namespace lexxos::hardware::modules::lyra {

struct UartId {
	int index;
	constexpr bool operator==(UartId const&) const = default;
};

struct SpiId {
	int index;
	constexpr bool operator==(SpiId const&) const = default;
};

struct I2cId {
	int index;
	constexpr bool operator==(I2cId const&) const = default;
};

// Physical SoC GPIO identity, independent of RM_IO aliases and consumers.
struct Pin {
	int bank;
	int number;
	constexpr bool operator==(Pin const&) const = default;
};

struct UartInterface {
	UartId id;
	// Pads occupied by this route. UART0 is inherited from U-Boot; TX/RX
	// direction is intentionally not inferred from SWD pin labels.
	std::array<Pin, 2> pads;
	constexpr bool operator==(UartInterface const&) const = default;
};

struct SpiInterface {
	SpiId id;
	Pin clock;
	Pin mosi;
	std::optional<Pin> miso;
	constexpr bool operator==(SpiInterface const&) const = default;
};

struct I2cInterface {
	I2cId id;
	Pin sda;
	Pin scl;
	constexpr bool operator==(I2cInterface const&) const = default;
};

struct Interfaces {
	std::span<UartInterface const> uarts;
	std::span<SpiInterface const> spis;
	std::span<I2cInterface const> i2cs;
};

// Available routes on Lyra, not a list of every controller inside RK3506.
// Evidence: debug_uart1.cpp, picocalc_lcd_spi0.cpp, picocalc_i2c_bus.cpp,
// picosd_spi1_hw.cpp. UART1 and I2C2 are alternative routes on the same pads.
inline constexpr std::array uartInterfaces{
    UartInterface{{0}, {{{0, 22}, {0, 23}}}},
    UartInterface{{1}, {{{0, 10}, {0, 11}}}},
};

inline constexpr std::array spiInterfaces{
    SpiInterface{{0},  {0, 7},  {0, 6}, std::nullopt},
    SpiInterface{{1}, {1, 25}, {1, 19},   Pin{1, 27}},
};

inline constexpr std::array i2cInterfaces{
    I2cInterface{{2}, {0, 11}, {0, 10}},
};

inline constexpr Interfaces interfaces{uartInterfaces, spiInterfaces, i2cInterfaces};

template <typename Interface, typename Id>
[[nodiscard]] constexpr Interface const* findInterface(std::span<Interface const> entries, Id id) {
	for (auto const& entry : entries) {
		if (entry.id == id) {
			return &entry;
		}
	}

	return nullptr;
}

[[nodiscard]] constexpr bool validPin(Pin pin) {
	return pin.bank >= 0 && pin.bank <= 1 && pin.number >= 0 && pin.number < 32;
}

template <typename Interface>
[[nodiscard]] constexpr bool uniqueIds(std::span<Interface const> entries) {
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (entries[i].id.index < 0) {
			return false;
		}

		for (std::size_t j = 0; j < i; ++j) {
			if (entries[i].id == entries[j].id) {
				return false;
			}
		}
	}

	return true;
}

[[nodiscard]] constexpr bool validInterfaces(Interfaces const& available) {
	if (!uniqueIds(available.uarts) || !uniqueIds(available.spis) || !uniqueIds(available.i2cs)) {
		return false;
	}

	for (auto const& uart : available.uarts) {
		if (!validPin(uart.pads[0]) || !validPin(uart.pads[1]) || uart.pads[0] == uart.pads[1]) {
			return false;
		}
	}

	for (auto const& spi : available.spis) {
		if (!validPin(spi.clock) || !validPin(spi.mosi) || spi.clock == spi.mosi || (spi.miso && (!validPin(*spi.miso) || *spi.miso == spi.clock || *spi.miso == spi.mosi))) {
			return false;
		}
	}

	for (auto const& i2c : available.i2cs) {
		if (!validPin(i2c.sda) || !validPin(i2c.scl) || i2c.sda == i2c.scl) {
			return false;
		}
	}

	return true;
}

// A product may expose a subset, but cannot invent a route unsupported by
// this module. To add a real route, extend the module inventory with evidence.
template <typename Interface>
[[nodiscard]] constexpr bool supportedRoutes(std::span<Interface const> selected, std::span<Interface const> supported) {
	for (auto const& route : selected) {
		auto const* known = findInterface(supported, route.id);
		if (!known || *known != route) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] constexpr bool supportedInterfaces(Interfaces const& selected) {
	return validInterfaces(selected)
	    && supportedRoutes(selected.uarts, interfaces.uarts)
	    && supportedRoutes(selected.spis, interfaces.spis)
	    && supportedRoutes(selected.i2cs, interfaces.i2cs);
}

static_assert(validInterfaces(interfaces));

} // namespace lexxos::hardware::modules::lyra
