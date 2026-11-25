#pragma once

#include <cstdint>

#include <lexxos/hardware/products/lyra_endpoints.h>

namespace lexxos::devices {

class I2cBus;

} // namespace lexxos::devices

namespace lexxos::composition::products {

// Stage-specific bindings. The resolver returns the already-owned controller;
// composition never creates a second bus for an alias such as debug/system.
struct LyraDeviceProviders {
	void* context;
	devices::I2cBus* (*i2c)(void*, hardware::modules::lyra::I2cId);
	void (*input)(void*, devices::I2cBus&);
	void (*display)(void*, hardware::products::LyraDisplayEndpoint const&, std::uint32_t);
};

enum class LyraCompositionResult {
	ProvidersInvoked,
	NoDevices,
	InvalidProduct,
	MissingProvider,
	MissingBus,
};

// Validates every required binding before invoking a provider. Bus readiness
// is deliberately not required: the existing southbridge service retries it.
LyraCompositionResult composeLyraDevices(
    hardware::products::LyraProduct const& product, bool displayEnabled,
    std::uint32_t displayHz, LyraDeviceProviders const& providers);

} // namespace lexxos::composition::products
