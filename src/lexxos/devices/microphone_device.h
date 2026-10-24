#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device.h>

namespace lexxos::devices {

struct MicrophonePcmCaptureRequest {
	AudioPcmFrame* frames{nullptr};
	std::size_t frameCapacity{0};
	std::size_t* framesRead{nullptr};
	std::uint32_t sampleRateHz{0};
	std::uint32_t timeoutMs{1000};
};

class MicrophoneDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override {
		return DeviceKind::Microphone;
	}

	[[nodiscard]] virtual bool supportsPcmCapture() const {
		return false;
	}

	[[nodiscard]] virtual std::uint32_t preferredPcmSampleRateHz() const {
		return 0;
	}

	virtual bool capturePcm(MicrophonePcmCaptureRequest const& request) {
		(void) request;
		return false;
	}
};

} // namespace lexxos::devices
