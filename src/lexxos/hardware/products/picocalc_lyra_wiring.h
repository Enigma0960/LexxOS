#pragma once

#include <array>
#include <cstdint>

#include <lexxos/hardware/modules/lyra_interfaces.h>
#include <lexxos/hardware/products/lyra_pin.h>

namespace lexxos::hardware::products {

// Device endpoints reference physical interfaces; bus signal pins live only
// in the module's interface catalog. CS/DC/reset belong to device endpoints.
struct PicoCalcLyraWiring {
	modules::lyra::I2cId southbridgeBus{2};
	std::uint32_t southbridgeI2cHz{100000};
	modules::lyra::SpiId displayBus{0};
	modules::lyra::SpiId storageBus{1};
	LyraPin displayCs{LyraPinDomain::Gpio0, 4};
	LyraPin displayDc{LyraPinDomain::Gpio0, 3};
	LyraPin displayReset{LyraPinDomain::Gpio0, 2};
	modules::lyra::Pin storageCs{1, 26};
};

inline constexpr PicoCalcLyraWiring picoCalcLyraWiring{};

enum class LyraWiringError : std::uint8_t {
	None,
	InvalidOrConflictingPin,
	UnsupportedController,
	UnsupportedFrequency,
	UnsupportedPinmux,
	MissingReceiveSignal,
};

[[nodiscard]] constexpr LyraWiringError validatePicoCalcLyraWiring(
    PicoCalcLyraWiring const& wiring, modules::lyra::Interfaces const& available = modules::lyra::interfaces) {
	auto const* i2c = modules::lyra::findInterface(available.i2cs, wiring.southbridgeBus);
	auto const* lcd = modules::lyra::findInterface(available.spis, wiring.displayBus);
	auto const* sd = modules::lyra::findInterface(available.spis, wiring.storageBus);

	if (!i2c || !lcd || !sd) {
		return LyraWiringError::UnsupportedController;
	}

	if (!sd->miso) {
		return LyraWiringError::MissingReceiveSignal;
	}

	if (wiring.southbridgeI2cHz != 100000) {
		return LyraWiringError::UnsupportedFrequency;
	}

	auto const cs = lyraGpio0Pin(wiring.displayCs);
	auto const dc = lyraGpio0Pin(wiring.displayDc);
	auto const reset = lyraGpio0Pin(wiring.displayReset);
	if (!cs || !dc || !reset) {
		return LyraWiringError::InvalidOrConflictingPin;
	}

	std::array pins{
	    i2c->sda, i2c->scl, lcd->clock, lcd->mosi,
	    sd->clock, sd->mosi, *sd->miso, wiring.storageCs,
	    modules::lyra::Pin{0,    *cs},
	    modules::lyra::Pin{0,    *dc},
	    modules::lyra::Pin{0, *reset}
    };

	for (std::size_t i = 0; i < pins.size(); ++i) {
		if (!modules::lyra::validPin(pins[i])) {
			return LyraWiringError::InvalidOrConflictingPin;
		}

		for (std::size_t j = 0; j < i; ++j) {
			if (pins[i] == pins[j]) {
				return LyraWiringError::InvalidOrConflictingPin;
			}
		}
	}

	// These control lines are still hard-coded in the existing device backends.
	if (*cs != 4 || *dc != 3 || *reset != 2 || wiring.storageCs != modules::lyra::Pin{1, 26}) {
		return LyraWiringError::UnsupportedPinmux;
	}

	return LyraWiringError::None;
}

// Compatibility predicate; complete validation also resolves interface references.
[[nodiscard]] constexpr bool hasValidDistinctPins(PicoCalcLyraWiring const& wiring) {
	auto const result = validatePicoCalcLyraWiring(wiring);
	return result == LyraWiringError::None || result == LyraWiringError::UnsupportedPinmux;
}

static_assert(validatePicoCalcLyraWiring(picoCalcLyraWiring) == LyraWiringError::None);

} // namespace lexxos::hardware::products
