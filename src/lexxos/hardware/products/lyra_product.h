#pragma once

#include <span>
#include <string_view>

#include <lexxos/hardware/products/picocalc_lyra_wiring.h>
#include <lexxos/hardware/products/storage_policy.h>

namespace lexxos::hardware::products {

enum class LyraCarrier {
	None,
	PicoCalc,
};

// Declaration-only product composition. Runtime providers are not selected here.
// Module revisions, voltage and power dependencies await a verified schematic.
// All pointers/spans borrow data; production declarations use static storage.
struct LyraProduct {
	char const* id;
	char const* name;
	LyraCarrier carrier;
	PicoCalcLyraWiring const* picoCalcWiring;
	std::span<StoragePolicy const> storage;
	modules::lyra::Interfaces available{modules::lyra::interfaces};
	modules::lyra::UartId console{0};
};

enum class LyraProductError {
	None,
	InvalidIdentity,
	InvalidInterfaces,
	UnknownConsole,
	InterfaceConflict,
	InvalidCarrier,
	CarrierWiringMismatch,
	InvalidWiring,
	InvalidStoragePolicy,
	StorageCompositionMismatch,
};

[[nodiscard]] constexpr LyraProductError validateLyraProduct(LyraProduct const& product) {
	if (product.id == nullptr || product.name == nullptr || std::string_view{product.id}.empty() || std::string_view{product.name}.empty()) {
		return LyraProductError::InvalidIdentity;
	}

	if (product.carrier != LyraCarrier::None && product.carrier != LyraCarrier::PicoCalc) {
		return LyraProductError::InvalidCarrier;
	}

	if (!modules::lyra::supportedInterfaces(product.available)) {
		return LyraProductError::InvalidInterfaces;
	}

	auto const* console = modules::lyra::findInterface(product.available.uarts, product.console);
	if (!console) {
		return LyraProductError::UnknownConsole;
	}

	bool const hasPicoCalc = product.carrier == LyraCarrier::PicoCalc;
	if (hasPicoCalc != (product.picoCalcWiring != nullptr)) {
		return LyraProductError::CarrierWiringMismatch;
	}

	if (hasPicoCalc && validatePicoCalcLyraWiring(*product.picoCalcWiring, product.available) != LyraWiringError::None) {
		return LyraProductError::InvalidWiring;
	}

	if (hasPicoCalc) {
		auto const& wiring = *product.picoCalcWiring;
		auto const* i2c = modules::lyra::findInterface(product.available.i2cs, wiring.southbridgeBus);
		auto const* lcd = modules::lyra::findInterface(product.available.spis, wiring.displayBus);
		auto const* sd = modules::lyra::findInterface(product.available.spis, wiring.storageBus);

		std::array used{
		    i2c->sda, i2c->scl, lcd->clock, lcd->mosi,
		    sd->clock, sd->mosi, *sd->miso, wiring.storageCs,
		    modules::lyra::Pin{0,    *lyraGpio0Pin(wiring.displayCs)},
		    modules::lyra::Pin{0,    *lyraGpio0Pin(wiring.displayDc)},
		    modules::lyra::Pin{0, *lyraGpio0Pin(wiring.displayReset)}
        };

		for (auto pin : used) {
			for (auto pad : console->pads) {
				if (pin == pad) {
					return LyraProductError::InterfaceConflict;
				}
			}
		}
	}

	if (validateStoragePolicy(product.storage) != StoragePolicyError::None) {
		return LyraProductError::InvalidStoragePolicy;
	}

	// Only the existing boot SDMMC and optional PicoCalc socket are migrated.
	if (product.storage.size() != (hasPicoCalc ? 2U : 1U)
	    || product.storage[0].role != StorageRole::System
	    || std::string_view{product.storage[0].deviceName} != "sd0"
	    || (hasPicoCalc && (product.storage[1].role != StorageRole::Removable || std::string_view{product.storage[1].deviceName} != "sd1"))) {
		return LyraProductError::StorageCompositionMismatch;
	}

	return LyraProductError::None;
}

} // namespace lexxos::hardware::products
