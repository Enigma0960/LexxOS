#pragma once

#include <optional>

#include <lexxos/hardware/legacy/lyra_display_pins.h>
#include <lexxos/hardware/legacy/lyra_i2c_pins.h>
#include <lexxos/hardware/modules/lyra.h>
#include <lexxos/hardware/modules/picocalc.h>
#include <lexxos/hardware/products/lyra_products.h>

namespace lexxos::hardware::legacy {

// One-way compatibility boundary: module/product data never include board.h.
// Feature and clock overrides are supplied by the existing platform adapter.
// Returned strings borrow product/policy storage, as in the legacy descriptor.
[[nodiscard]] constexpr std::optional<platform::LegacyBoardDescriptor> tryLyraBoard(
    products::LyraProduct const& product, bool displayEnabled, std::uint32_t displayHz = products::picoCalcLyraDefaultDisplayHz) {
	auto const endpoints = products::resolveLyraEndpoints(product);
	if (!endpoints
	    || product.storage.size() > platform::maxBoardStorageSlots) {
		return std::nullopt;
	}

	platform::LegacyBoardDescriptor result{};
	result.boardId = product.id;
	result.boardName = product.name;
	result.vendor = modules::lyra::vendor;
	result.processorFamily = platform::ProcessorFamily::Rk3506;
	result.processorName = modules::lyra::processorName;
	result.interfaces.consoleUart = {
	    .index = product.console.index,
	    .baudRate = modules::lyra::bootConsoleBaudRate,
	};
	if (product.carrier == products::LyraCarrier::PicoCalc) {
		result.interfaces.systemI2c = lyraI2cPins(*endpoints->southbridge);

		if (displayEnabled) {
			result.spiTftDisplay = lyraDisplayPins(*endpoints->display, displayHz);
		}
	}

	for (auto const& slot : product.storage) {
		result.storageSlots[result.storageSlotCount++] = {
		    .deviceName = slot.deviceName,
		    .mountPath = slot.mountPath,
		    .role = slot.role == products::StorageRole::System
		        ? platform::SdMediaRole::System
		        : platform::SdMediaRole::Removable,
		    .hotSwappable = slot.hotSwappable,
		    .autoMount = slot.autoMount,
		    .readOnlyByDefault = slot.readOnlyByDefault,
		};
	}

	return result;
}

// Known declarations are validated at compile time; runtime callers can use
// tryLyraBoard to reject invalid data without creating a partial descriptor.
static_assert(tryLyraBoard(products::picoCalcLyraProduct, true).has_value());
static_assert(tryLyraBoard(products::lyraStandaloneProduct, false).has_value());

[[nodiscard]] constexpr platform::LegacyBoardDescriptor picoCalcLyraBoard(
    bool displayEnabled, std::uint32_t displayHz = products::picoCalcLyraDefaultDisplayHz) {
	return *tryLyraBoard(products::picoCalcLyraProduct, displayEnabled, displayHz);
}

[[nodiscard]] constexpr platform::LegacyBoardDescriptor lyraStandaloneBoard() {
	return *tryLyraBoard(products::lyraStandaloneProduct, false);
}

} // namespace lexxos::hardware::legacy
