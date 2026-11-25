#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio_device.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/clocks.h>
	#include <hardware/gpio.h>
	#include <hardware/pwm.h>
	#include <pico/platform.h>
	#include <pico/time.h>
#endif

namespace lexxos::devices {

class PicoPwmAudioDevice final : public AudioDevice {
public:
	PicoPwmAudioDevice(lexxos::platform::AudioBeepPins config, lexxos::platform::PlatformServices const& platform);

	[[nodiscard]] std::string_view name() const override;

	[[nodiscard]] bool initialize();

	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] bool supportsChannel(AudioChannel channel) const override;

	[[nodiscard]] bool supportsToneMix() const override;

	[[nodiscard]] std::size_t maxToneMixVoices() const override;

	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t maxToneDurationMs() const override;

	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override;

	bool playTone(AudioToneRequest const& request) override;

	bool playToneMix(AudioToneMixRequest const& request) override;

private:
	static constexpr std::uint32_t minFrequencyHz = 20;
	static constexpr std::uint32_t maxFrequencyHz = 20000;
	static constexpr std::uint32_t defaultFrequencyHz = 880;
	static constexpr std::uint32_t defaultDurationMs = 120;
	static constexpr std::uint32_t maxDurationMs = 5000;
	static constexpr std::size_t maxMixVoices = 6;
	static constexpr std::size_t maxMixPhaseSlots = 32;
	static constexpr std::uint16_t pwmWrap = 1000;

#if defined(LEXXOS_PLATFORM_PICO)
	static constexpr std::uint32_t mixSampleRateHz = 20000;
	static constexpr std::uint32_t mixSamplePeriodUs = 50;
	static constexpr std::uint32_t mixRampMs = 0;
	static constexpr std::uint8_t mixDefaultMasterVolume = 128;
	static constexpr std::uint16_t mixPwmWrap = 255;
	static constexpr std::uint16_t mixPwmCenter = 128;

	struct PwmPin {
		lexxos::platform::GpioPin pin;
		uint gpio{0};
		uint slice{0};
		bool valid{false};
	};

	struct MixPhaseSlot;

	struct MixVoice {
		std::uint32_t phase{0};
		std::uint32_t phaseStep{0};
		std::uint32_t noise{0x12345678U};
		std::uint32_t frequencyHz{0};
		MixPhaseSlot* phaseSlot{nullptr};
		std::uint16_t tag{0};
		std::uint8_t volume{0};
		std::uint8_t pan{64};
		AudioToneWaveform waveform{AudioToneWaveform::Square};
	};

	struct MixPhaseSlot {
		std::uint32_t frequencyHz{0};
		std::uint32_t phase{0};
		std::uint32_t noise{0x12345678U};
		std::uint16_t tag{0};
		AudioToneWaveform waveform{AudioToneWaveform::Square};
		bool valid{false};
	};

	[[nodiscard]] static uint picoGpio(lexxos::platform::GpioPin pin);

	void sleepMs(std::uint32_t milliseconds) const;

	static void stopPin(lexxos::platform::GpioPin pin);

	void stopConfiguredPins() const;

	void stopMixOutput();

	void settleMixOutput(std::uint32_t milliseconds);

	void appendPin(PwmPin* pins, std::uint32_t& count, lexxos::platform::GpioPin pin) const;

	std::uint32_t collectPins(AudioChannel channel, PwmPin* pins) const;

	static bool hasSlice(PwmPin const* pins, std::uint32_t count, std::uint32_t until, uint slice);

	static float pwmDividerFor(std::uint32_t frequencyHz);

	static std::uint32_t phaseStepFor(std::uint32_t frequencyHz);

	static std::uint16_t fallbackVoiceTag(AudioToneVoice const& voice, std::size_t index);

	MixPhaseSlot* phaseSlotFor(std::uint16_t tag, AudioToneWaveform waveform, std::uint32_t frequencyHz);

	static std::int16_t waveformSample(AudioToneWaveform waveform, std::uint32_t phase, std::uint32_t& noise);

	static void panGains(std::uint8_t pan, std::int32_t& left, std::int32_t& right);

	static std::uint16_t mixLevel(std::int32_t sum, std::uint8_t masterVolume, std::size_t voiceCount);

	static std::uint16_t smoothLevel(std::uint16_t target, std::uint16_t& state);

	static std::uint8_t envelopeGain(std::uint64_t sample, std::uint64_t samples);

	bool playToneOnPico(AudioToneRequest const& request);

	bool playToneMixOnPico(AudioToneMixRequest const& request);
#else
	void stopConfiguredPins() const;
#endif

	lexxos::platform::AudioBeepPins _config{};
	lexxos::platform::PlatformServices const* _platform{nullptr};
#if defined(LEXXOS_PLATFORM_PICO)
	MixPhaseSlot _mixPhaseSlots[maxMixPhaseSlots]{};
	std::size_t _nextMixPhaseSlot{0};
	std::uint16_t _mixLeftLevel{mixPwmCenter};
	std::uint16_t _mixRightLevel{mixPwmCenter};
	AudioChannel _mixChannel{AudioChannel::Stereo};
	bool _mixConfigured{false};
#endif
	bool _ready{false};
};

} // namespace lexxos::devices
