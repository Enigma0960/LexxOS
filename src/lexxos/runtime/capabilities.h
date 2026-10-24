#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::devices {
class DeviceManager;
} // namespace lexxos::devices

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::runtime {

enum class Capability : std::uint32_t {
	Display = 1U << 0U,
	DisplayRgb565 = 1U << 1U,
	DisplayPowerMode = 1U << 2U,
	StorageBlock = 1U << 3U,
	StorageRemovable = 1U << 4U,
	InputKeyboard = 1U << 5U,
	InputGamepad = 1U << 6U,
	AudioTone = 1U << 7U,
	AudioToneMix = 1U << 8U,
	WirelessWifi = 1U << 9U,
	WirelessBluetooth = 1U << 10U,
	FlashNamedRegions = 1U << 11U,
	ExternalPsram = 1U << 12U,
	PicoCalcSouthbridge = 1U << 13U,
	GpioUser = 1U << 14U,
	PowerReboot = 1U << 15U,
	PowerBattery = 1U << 16U,
	RtcSoftware = 1U << 17U,
	InputPointer = 1U << 18U,
	AudioPcm = 1U << 19U,
};

struct CapabilityDescriptor {
	Capability capability;
	std::string_view name;
};

inline constexpr CapabilityDescriptor knownCapabilities[] = {
    {            .capability = Capability::Display,               .name = "display"},
    {      .capability = Capability::DisplayRgb565,        .name = "display.rgb565"},
    {   .capability = Capability::DisplayPowerMode,    .name = "display.power-mode"},
    {       .capability = Capability::StorageBlock,         .name = "storage.block"},
    {   .capability = Capability::StorageRemovable,     .name = "storage.removable"},
    {      .capability = Capability::InputKeyboard,        .name = "input.keyboard"},
    {       .capability = Capability::InputGamepad,         .name = "input.gamepad"},
    {          .capability = Capability::AudioTone,            .name = "audio.tone"},
    {       .capability = Capability::AudioToneMix,        .name = "audio.tone-mix"},
    {           .capability = Capability::AudioPcm,             .name = "audio.pcm"},
    {       .capability = Capability::WirelessWifi,         .name = "wireless.wifi"},
    {  .capability = Capability::WirelessBluetooth,    .name = "wireless.bluetooth"},
    {  .capability = Capability::FlashNamedRegions,   .name = "flash.named-regions"},
    {      .capability = Capability::ExternalPsram, .name = "memory.external.psram"},
    {.capability = Capability::PicoCalcSouthbridge,  .name = "picocalc.southbridge"},
    {           .capability = Capability::GpioUser,             .name = "gpio.user"},
    {        .capability = Capability::PowerReboot,          .name = "power.reboot"},
    {       .capability = Capability::PowerBattery,         .name = "power.battery"},
    {        .capability = Capability::RtcSoftware,          .name = "rtc.software"},
    {       .capability = Capability::InputPointer,         .name = "input.pointer"},
};

class CapabilitySet {
public:
	constexpr CapabilitySet() = default;

	explicit constexpr CapabilitySet(std::uint32_t bits)
	    : _bits(bits) {
	}

	[[nodiscard]] constexpr std::uint32_t bits() const {
		return _bits;
	}

	[[nodiscard]] constexpr bool empty() const {
		return _bits == 0;
	}

	[[nodiscard]] constexpr bool has(Capability capability) const {
		return (_bits & bit(capability)) != 0;
	}

	[[nodiscard]] constexpr bool hasAll(CapabilitySet required) const {
		return (_bits & required._bits) == required._bits;
	}

	constexpr void add(Capability capability) {
		_bits |= bit(capability);
	}

	constexpr void remove(Capability capability) {
		_bits &= ~bit(capability);
	}

private:
	[[nodiscard]] static constexpr std::uint32_t bit(Capability capability) {
		return static_cast<std::uint32_t>(capability);
	}

	std::uint32_t _bits{0};
};

[[nodiscard]] constexpr std::string_view capabilityName(Capability capability) {
	for (auto const& descriptor : knownCapabilities) {
		if (descriptor.capability == capability) {
			return descriptor.name;
		}
	}
	return "unknown";
}

[[nodiscard]] CapabilitySet detectCapabilities(
    lexxos::devices::DeviceManager const& devices,
    lexxos::platform::PlatformServices const& platform);

} // namespace lexxos::runtime
