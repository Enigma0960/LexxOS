#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

enum class AudioChannel : std::uint8_t {
	Left,
	Right,
	Stereo,
};

enum class AudioToneWaveform : std::uint8_t {
	Sine,
	Triangle,
	Saw,
	Square,
	Pulse,
	Noise,
};

[[nodiscard]] constexpr std::string_view toString(AudioChannel channel) {
	switch (channel) {
		case AudioChannel::Left:
			return "left";
		case AudioChannel::Right:
			return "right";
		case AudioChannel::Stereo:
			return "stereo";
	}
	return "unknown";
}

[[nodiscard]] constexpr std::string_view toString(AudioToneWaveform waveform) {
	switch (waveform) {
		case AudioToneWaveform::Sine:
			return "sine";
		case AudioToneWaveform::Triangle:
			return "triangle";
		case AudioToneWaveform::Saw:
			return "saw";
		case AudioToneWaveform::Square:
			return "square";
		case AudioToneWaveform::Pulse:
			return "pulse";
		case AudioToneWaveform::Noise:
			return "noise";
	}
	return "unknown";
}

struct AudioToneRequest {
	AudioChannel channel{AudioChannel::Stereo};
	std::uint32_t frequencyHz{880};
	std::uint32_t durationMs{120};
	std::uint8_t volume{128};
};

struct AudioToneVoice {
	std::uint32_t frequencyHz{880};
	std::uint16_t tag{0};
	std::uint8_t volume{128};
	std::uint8_t pan{64};
	AudioToneWaveform waveform{AudioToneWaveform::Square};
};

struct AudioToneMixRequest {
	AudioChannel channel{AudioChannel::Stereo};
	AudioToneVoice const* voices{nullptr};
	std::size_t voiceCount{0};
	std::uint32_t durationMs{120};
	std::uint8_t masterVolume{255};
};

struct AudioPcmFrame {
	std::int16_t left{0};
	std::int16_t right{0};
};

struct AudioPcmBufferRequest {
	AudioChannel channel{AudioChannel::Stereo};
	AudioPcmFrame const* frames{nullptr};
	std::size_t frameCount{0};
	std::uint32_t sampleRateHz{22050};
};

struct AudioPcmOutputStats {
	std::uint64_t framesQueued{0};
	std::uint64_t framesWritten{0};
	std::uint32_t bufferCalls{0};
	std::uint32_t lateFrames{0};
	std::uint32_t maxLatenessUs{0};
	std::uint32_t resyncs{0};
	std::uint32_t underruns{0};
	std::uint32_t fullWaits{0};
	std::uint32_t ringCapacity{0};
	std::uint32_t ringFill{0};
	std::uint32_t ringHighWatermark{0};
	std::uint32_t sampleRateHz{0};
};

class AudioDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override {
		return DeviceKind::Audio;
	}

	[[nodiscard]] virtual bool supportsTone() const {
		return true;
	}

	[[nodiscard]] virtual bool supportsToneMix() const {
		return false;
	}

	[[nodiscard]] virtual bool supportsPcmBuffer() const {
		return false;
	}

	[[nodiscard]] virtual std::uint32_t preferredPcmSampleRateHz() const {
		return 0;
	}

	[[nodiscard]] virtual std::size_t maxToneMixVoices() const {
		return 1;
	}

	[[nodiscard]] virtual bool supportsChannel(AudioChannel channel) const = 0;
	[[nodiscard]] virtual std::uint32_t minToneFrequencyHz() const = 0;
	[[nodiscard]] virtual std::uint32_t maxToneFrequencyHz() const = 0;
	[[nodiscard]] virtual std::uint32_t defaultToneFrequencyHz() const = 0;
	[[nodiscard]] virtual std::uint32_t maxToneDurationMs() const = 0;
	[[nodiscard]] virtual std::uint32_t defaultToneDurationMs() const = 0;

	virtual bool playTone(AudioToneRequest const& request) = 0;

	virtual bool playToneMix(AudioToneMixRequest const& request) {
		(void) request;
		return false;
	}

	virtual bool playPcmBuffer(AudioPcmBufferRequest const& request) {
		(void) request;
		return false;
	}

	virtual bool drainPcmOutput(std::uint32_t timeoutMs) {
		(void) timeoutMs;
		return true;
	}

	virtual void stopPcmOutput() {
	}

	virtual void resetPcmOutputStats() {
	}

	[[nodiscard]] virtual bool pcmOutputStats(AudioPcmOutputStats& stats) const {
		stats = AudioPcmOutputStats{};
		return false;
	}
};

} // namespace lexxos::devices
