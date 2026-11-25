#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio/pico_pwm_audio_device.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/gpio.h>
	#include <hardware/pwm.h>
	#include <pico/sync.h>
	#include <pico/time.h>
#endif

namespace lexxos::devices {

class PicoPwmPcmAudioDevice final : public AudioDevice {
public:
	PicoPwmPcmAudioDevice(lexxos::platform::AudioBeepPins config, lexxos::platform::PlatformServices const& platform);

	[[nodiscard]] std::string_view name() const override;

	[[nodiscard]] bool initialize();

	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] bool supportsTone() const override;

	[[nodiscard]] bool supportsToneMix() const override;

	[[nodiscard]] bool supportsPcmBuffer() const override;

	[[nodiscard]] std::size_t maxToneMixVoices() const override;

	[[nodiscard]] bool supportsChannel(AudioChannel channel) const override;

	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override;

	[[nodiscard]] std::uint32_t maxToneDurationMs() const override;

	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override;

	bool playTone(AudioToneRequest const& request) override;

	bool playToneMix(AudioToneMixRequest const& request) override;

	bool playPcmBuffer(AudioPcmBufferRequest const& request) override;

	bool drainPcmOutput(std::uint32_t timeoutMs) override;

	void stopPcmOutput() override;

	void resetPcmOutputStats() override;

	[[nodiscard]] bool pcmOutputStats(AudioPcmOutputStats& stats) const override;

private:
	static constexpr std::uint32_t minPcmSampleRateHz = 8000;
	static constexpr std::uint32_t maxPcmSampleRateHz = 48000;
	static constexpr std::uint16_t pcmPwmWrap = 255;
	static constexpr std::uint16_t pcmPwmCenter = 128;

#if defined(LEXXOS_PLATFORM_PICO)
	static constexpr std::size_t pcmRingCapacity = 4096;
	static constexpr std::size_t pcmRingStartThreshold = 512;

	struct PwmPin {
		lexxos::platform::GpioPin pin;
		uint gpio{0};
		uint slice{0};
		bool valid{false};
	};

	[[nodiscard]] static uint picoGpio(lexxos::platform::GpioPin pin);

	static void stopPin(lexxos::platform::GpioPin pin);

	void appendPin(PwmPin* pins, std::uint32_t& count, lexxos::platform::GpioPin pin) const;

	std::uint32_t collectPins(AudioChannel channel, PwmPin* pins) const;

	static bool hasSlice(PwmPin const* pins, std::uint32_t count, std::uint32_t until, uint slice);

	static std::uint16_t pcmToPwmLevel(std::int16_t sample);

	void invalidatePcmConfiguration();

	void configurePcmOutput(AudioChannel channel, std::uint32_t sampleRateHz, PwmPin const* pins, std::uint32_t pinCount);

	void setPcmPinsToCenter();

	void resetPcmQueue();

	[[nodiscard]] std::size_t queuedFrames() const;

	[[nodiscard]] bool enqueueFrame(AudioPcmFrame const& frame);

	[[nodiscard]] bool dequeueFrame(AudioPcmFrame& frame);

	void recordFullWait();

	void recordUnderrun();

	void recordWrittenFrame();

	static bool pcmTimerCallback(repeating_timer_t* timer);

	void writeNextPcmFrameFromTimer();

	bool startPcmTimer(std::uint32_t sampleRateHz);

	void stopPcmTimer();

	bool playPcmBufferOnPico(AudioPcmBufferRequest const& request);
#endif

	PicoPwmAudioDevice _toneDevice;
	lexxos::platform::AudioBeepPins _config{};
	lexxos::platform::PlatformServices const* _platform{nullptr};
#if defined(LEXXOS_PLATFORM_PICO)
	mutable critical_section_t _pcmLock{};
	repeating_timer_t _pcmTimer{};
	AudioPcmFrame _pcmRing[pcmRingCapacity]{};
	PwmPin _pcmPins[2]{};
	AudioPcmOutputStats _pcmStats{};
	AudioChannel _pcmChannel{AudioChannel::Stereo};
	std::size_t _pcmReadIndex{0};
	std::size_t _pcmWriteIndex{0};
	std::size_t _pcmFill{0};
	std::uint32_t _pcmPinCount{0};
	std::uint32_t _pcmSampleRateHz{0};
	std::uint32_t _pcmTimerSampleRateHz{0};
	bool _pcmConfigured{false};
	bool _pcmTimerRunning{false};
	bool _pcmLockInitialized{false};
#endif
	bool _ready{false};
};

} // namespace lexxos::devices
