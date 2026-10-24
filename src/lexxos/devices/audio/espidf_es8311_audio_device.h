#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/microphone_device.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

class I2cBus;

class EspIdfEs8311AudioDevice final : public AudioDevice {
public:
	EspIdfEs8311AudioDevice(
	    lexxos::platform::AudioI2sCodecPins config,
	    I2cBus& controlBus,
	    lexxos::platform::PlatformServices const& platform);

	~EspIdfEs8311AudioDevice() override;

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool initialize();
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] bool supportsPcmBuffer() const override;
	[[nodiscard]] std::uint32_t preferredPcmSampleRateHz() const override;
	[[nodiscard]] bool supportsChannel(AudioChannel channel) const override;
	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override;
	[[nodiscard]] std::uint32_t maxToneDurationMs() const override;
	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override;

	bool playTone(AudioToneRequest const& request) override;
	bool playPcmBuffer(AudioPcmBufferRequest const& request) override;
	bool drainPcmOutput(std::uint32_t timeoutMs) override;
	void stopPcmOutput() override;
	void resetPcmOutputStats() override;
	[[nodiscard]] bool pcmOutputStats(AudioPcmOutputStats& stats) const override;

	[[nodiscard]] bool supportsPcmCapture() const;
	[[nodiscard]] std::uint32_t preferredCaptureSampleRateHz() const;
	[[nodiscard]] std::string_view microphoneName() const;
	bool capturePcm(MicrophonePcmCaptureRequest const& request);

private:
	[[nodiscard]] bool _configureAmplifier();
	[[nodiscard]] bool _setAmplifierEnabled(bool enabled);
	[[nodiscard]] bool _initializeI2s();
	[[nodiscard]] bool _initializeCodec();
	[[nodiscard]] bool _writeCodecRegister(std::uint8_t reg, std::uint8_t value);
	[[nodiscard]] std::uint32_t _nativeSampleRateHz() const;
	[[nodiscard]] bool _ensureI2sSampleRate(std::uint32_t sampleRateHz);
	[[nodiscard]] bool _setCodecMute(bool mute);
	[[nodiscard]] bool _prepareOutput(std::uint32_t sampleRateHz);
	[[nodiscard]] bool _prepareInput();
	void _resetPcmResampler();
	[[nodiscard]] bool _flushResampledOutput();
	[[nodiscard]] bool _writePcmFramesResampled(AudioPcmFrame const* frames, std::size_t frameCount, AudioChannel channel, std::uint32_t inputSampleRateHz);
	[[nodiscard]] bool _writePcmFrames(AudioPcmFrame const* frames, std::size_t frameCount, AudioChannel channel, bool countStats);
	[[nodiscard]] bool _writeSilence(std::uint32_t frames);
	void _releaseI2s();
	void _sleepMs(std::uint32_t milliseconds) const;

	lexxos::platform::AudioI2sCodecPins _config{};
	I2cBus* _controlBus{nullptr};
	lexxos::platform::PlatformServices const* _platform{nullptr};
	AudioPcmOutputStats _pcmStats{};
	void* _txHandle{nullptr};
	void* _rxHandle{nullptr};
	std::uint32_t _currentSampleRateHz{0};
	std::uint32_t _pcmInputSampleRateHz{0};
	std::uint64_t _resamplePhaseQ32{0};
	std::uint64_t _resampleStepQ32{0};
	AudioPcmFrame _resamplePrevious{};
	std::array<AudioPcmFrame, 256> _resampleOutput{};
	std::size_t _resampleOutputCount{0};
	AudioChannel _resampleOutputChannel{AudioChannel::Stereo};
	bool _amplifierConfigured{false};
	bool _amplifierEnabled{false};
	bool _txEnabled{false};
	bool _rxEnabled{false};
	bool _codecMuted{true};
	bool _inputPrepared{false};
	bool _resampleHasPrevious{false};
	bool _ready{false};
};

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
