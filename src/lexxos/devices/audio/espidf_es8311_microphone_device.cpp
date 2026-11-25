#include <lexxos/devices/audio/espidf_es8311_microphone_device.h>

#include <lexxos/devices/audio/espidf_es8311_audio_device.h>

namespace lexxos::devices {

EspIdfEs8311MicrophoneDevice::EspIdfEs8311MicrophoneDevice(EspIdfEs8311AudioDevice& codec)
    : _codec(codec) {
}

std::string_view EspIdfEs8311MicrophoneDevice::name() const {
	return _codec.microphoneName();
}

bool EspIdfEs8311MicrophoneDevice::isReady() const {
	return _codec.isReady();
}

bool EspIdfEs8311MicrophoneDevice::supportsPcmCapture() const {
	return _codec.supportsPcmCapture();
}

std::uint32_t EspIdfEs8311MicrophoneDevice::preferredPcmSampleRateHz() const {
	return _codec.preferredCaptureSampleRateHz();
}

bool EspIdfEs8311MicrophoneDevice::capturePcm(MicrophonePcmCaptureRequest const& request) {
	return _codec.capturePcm(request);
}

} // namespace lexxos::devices
