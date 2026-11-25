#pragma once

#include <string_view>

#include <lexxos/devices/audio_device.h>

namespace lexxos::emulator {

class SimAudioDevice final : public lexxos::devices::AudioDevice {
public:
	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] bool supportsPcmBuffer() const override;
	[[nodiscard]] bool supportsChannel(lexxos::devices::AudioChannel channel) const override;
	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t maxToneDurationMs() const override;
	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override;

	bool playTone(lexxos::devices::AudioToneRequest const& request) override;
	bool playPcmBuffer(lexxos::devices::AudioPcmBufferRequest const& request) override;
	bool drainPcmOutput(std::uint32_t timeoutMs) override;
	void stopPcmOutput() override;
	void resetPcmOutputStats() override;
	[[nodiscard]] bool pcmOutputStats(lexxos::devices::AudioPcmOutputStats& stats) const override;
};

[[nodiscard]] SimAudioDevice& simAudioDevice();

} // namespace lexxos::emulator
