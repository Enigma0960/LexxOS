#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/microphone_device.h>

namespace lexxos::devices {

class EspIdfEs8311AudioDevice;

class EspIdfEs8311MicrophoneDevice final : public MicrophoneDevice {
public:
	explicit EspIdfEs8311MicrophoneDevice(EspIdfEs8311AudioDevice& codec);

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] bool supportsPcmCapture() const override;
	[[nodiscard]] std::uint32_t preferredPcmSampleRateHz() const override;
	bool capturePcm(MicrophonePcmCaptureRequest const& request) override;

private:
	EspIdfEs8311AudioDevice& _codec;
};

} // namespace lexxos::devices
