#include <lexxos/composition/detail/platform_device_providers.h>
#include <lexxos/composition/products/lyra_device_composition.h>
#include <lexxos/composition/products/lyra_i2c_bus.h>
#include <lexxos/composition/products/picocalc_lyra_device_providers.h>
#include <lexxos/composition/products/selected_lyra_configuration.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/hardware/legacy/lyra_i2c_pins.h>
#include <lexxos/platform/platform.h>

namespace lexxos::composition::detail {

namespace {

	struct ProviderContext {
		devices::DeviceManager& devices;
		platform::PlatformServices const& services;
	};

	devices::I2cBus* resolveI2c(void* opaque, hardware::modules::lyra::I2cId id) {
		auto& context = *static_cast<ProviderContext*>(opaque);
		auto const& endpoint = products::selectedLyraEndpoints.southbridge;
		if (!endpoint || endpoint->route.id != id) {
			return nullptr;
		}

		return context.devices.i2cBus("system");
	}

	void registerInput(void* opaque, devices::I2cBus& bus) {
		auto& context = *static_cast<ProviderContext*>(opaque);
		products::registerPicoCalcLyraInput(context.devices, context.services, bus);
	}

	void registerDisplay(void* opaque, hardware::products::LyraDisplayEndpoint const& endpoint, std::uint32_t frequencyHz) {
		auto& context = *static_cast<ProviderContext*>(opaque);
		products::registerPicoCalcLyraDisplay(context.devices, context.services, endpoint, frequencyHz);
	}

} // namespace

static void registerRk3506BoardDevices(devices::DeviceManager& devices,
    platform::PlatformServices const& platformServices, platform::LegacyBoardDescriptor const&) {
	ProviderContext context{devices, platformServices};
	// Both names refer to the same physical controller. Register even when
	// initialization fails so the existing southbridge retry path is retained.
	if (auto const& endpoint = products::selectedLyraEndpoints.southbridge; endpoint) {
		auto& bus = products::lyraSouthbridgeBus();
		if (!bus.isReady()) {
			(void) bus.initialize(hardware::legacy::lyraI2cPins(*endpoint));
		}

		devices.registerI2cBus("debug", "tools/debug/display I2C bus", bus);
		devices.registerI2cBus("system", "system board I2C bus", bus);
	}

	auto const result = products::composeLyraDevices(products::selectedLyraProduct,
	    products::selectedLyraDisplayEnabled, products::selectedLyraDisplayHz,
	    {&context, resolveI2c, registerInput, registerDisplay});

	if (result != products::LyraCompositionResult::ProvidersInvoked && result != products::LyraCompositionResult::NoDevices) {
		platform::writeConsole("Lyra: device composition rejected product or missing bus/provider\n");
	}
}

std::span<BoardDeviceProvider const> selectedBoardDeviceProviders() {
	static constexpr BoardDeviceProvider providers[]{registerRk3506BoardDevices};
	return providers;
}

} // namespace lexxos::composition::detail
