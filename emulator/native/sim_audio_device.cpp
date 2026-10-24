#include <sim_audio_device.h>

#include <sim_runtime.h>

namespace lexxos::emulator {
namespace {

	[[nodiscard]] SimRuntime* readyRuntime() {
		auto* active = activeRuntime();
		return active != nullptr && active->audioMode() != LEXXOS_SIM_AUDIO_OFF ? active : nullptr;
	}

} // namespace

std::string_view SimAudioDevice::name() const {
	auto* active = activeRuntime();
	if (active == nullptr) {
		return "emu_audio";
	}
	switch (active->audioMode()) {
		case LEXXOS_SIM_AUDIO_PWM:
			return "emu_pwm_audio";
		case LEXXOS_SIM_AUDIO_ADC:
			return "emu_adc_audio";
		case LEXXOS_SIM_AUDIO_OFF:
		default:
			return "emu_audio";
	}
}

bool SimAudioDevice::isReady() const {
	return readyRuntime() != nullptr;
}

bool SimAudioDevice::supportsPcmBuffer() const {
	return true;
}

bool SimAudioDevice::supportsChannel(lexxos::devices::AudioChannel channel) const {
	switch (channel) {
		case lexxos::devices::AudioChannel::Left:
		case lexxos::devices::AudioChannel::Right:
		case lexxos::devices::AudioChannel::Stereo:
			return true;
	}
	return false;
}

std::uint32_t SimAudioDevice::minToneFrequencyHz() const {
	return 20;
}

std::uint32_t SimAudioDevice::maxToneFrequencyHz() const {
	return 20000;
}

std::uint32_t SimAudioDevice::defaultToneFrequencyHz() const {
	return 880;
}

std::uint32_t SimAudioDevice::maxToneDurationMs() const {
	return 10000;
}

std::uint32_t SimAudioDevice::defaultToneDurationMs() const {
	return 120;
}

bool SimAudioDevice::playTone(lexxos::devices::AudioToneRequest const& request) {
	auto* active = readyRuntime();
	if (active == nullptr || !supportsChannel(request.channel)) {
		return false;
	}
	return active->audioPlayTone(request);
}

bool SimAudioDevice::playPcmBuffer(lexxos::devices::AudioPcmBufferRequest const& request) {
	auto* active = readyRuntime();
	if (active == nullptr || !supportsChannel(request.channel)) {
		return false;
	}
	return active->audioPlayPcmBuffer(request);
}

bool SimAudioDevice::drainPcmOutput(std::uint32_t timeoutMs) {
	auto* active = readyRuntime();
	return active != nullptr && active->audioDrainPcmOutput(timeoutMs);
}

void SimAudioDevice::stopPcmOutput() {
	if (auto* active = readyRuntime(); active != nullptr) {
		active->audioStopPcmOutput();
	}
}

void SimAudioDevice::resetPcmOutputStats() {
	if (auto* active = readyRuntime(); active != nullptr) {
		active->audioResetPcmOutputStats();
	}
}

bool SimAudioDevice::pcmOutputStats(lexxos::devices::AudioPcmOutputStats& stats) const {
	auto* active = readyRuntime();
	if (active == nullptr) {
		stats = {};
		return false;
	}
	return active->audioPcmOutputStats(stats);
}

SimAudioDevice& simAudioDevice() {
	static SimAudioDevice device;
	return device;
}

} // namespace lexxos::emulator
