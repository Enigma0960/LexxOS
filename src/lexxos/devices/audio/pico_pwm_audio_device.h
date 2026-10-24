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
	PicoPwmAudioDevice(lexxos::platform::AudioBeepPins config, lexxos::platform::PlatformServices const& platform)
	    : _config(config)
	    , _platform(&platform) {
	}

	[[nodiscard]] std::string_view name() const override {
		return "picocalc-audio";
	}

	[[nodiscard]] bool initialize() {
		_ready = _config.isValid();
#if defined(LEXXOS_PLATFORM_PICO)
		if (_ready) {
			stopConfiguredPins();
		}
#else
		_ready = false;
#endif
		return _ready;
	}

	[[nodiscard]] bool isReady() const override {
		return _ready;
	}

	[[nodiscard]] bool supportsChannel(AudioChannel channel) const override {
		switch (channel) {
			case AudioChannel::Left:
				return _config.hasLeft();
			case AudioChannel::Right:
				return _config.hasRight();
			case AudioChannel::Stereo:
				return _config.hasLeft() && _config.hasRight();
		}
		return false;
	}

	[[nodiscard]] bool supportsToneMix() const override {
		return true;
	}

	[[nodiscard]] std::size_t maxToneMixVoices() const override {
		return maxMixVoices;
	}

	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override {
		return minFrequencyHz;
	}

	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override {
		return maxFrequencyHz;
	}

	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override {
		return _config.defaultFrequencyHz == 0 ? defaultFrequencyHz : _config.defaultFrequencyHz;
	}

	[[nodiscard]] std::uint32_t maxToneDurationMs() const override {
		return maxDurationMs;
	}

	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override {
		return _config.defaultDurationMs == 0 ? defaultDurationMs : _config.defaultDurationMs;
	}

	bool playTone(AudioToneRequest const& request) override {
		if (!_ready || !supportsChannel(request.channel)) {
			return false;
		}
		if (request.frequencyHz < minFrequencyHz || request.frequencyHz > maxFrequencyHz) {
			return false;
		}
		if (request.durationMs == 0 || request.durationMs > maxDurationMs) {
			return false;
		}
#if defined(LEXXOS_PLATFORM_PICO)
		return playToneOnPico(request);
#else
		(void) request;
		return false;
#endif
	}

	bool playToneMix(AudioToneMixRequest const& request) override {
		if (!_ready || !supportsChannel(request.channel)) {
			return false;
		}
		if (request.durationMs == 0 || request.durationMs > maxDurationMs) {
			return false;
		}
		if (request.voiceCount != 0 && request.voices == nullptr) {
			return false;
		}
#if defined(LEXXOS_PLATFORM_PICO)
		return playToneMixOnPico(request);
#else
		(void) request;
		return false;
#endif
	}

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

	[[nodiscard]] static uint picoGpio(lexxos::platform::GpioPin pin) {
		return static_cast<uint>(pin.number);
	}

	void sleepMs(std::uint32_t milliseconds) const {
		if (_platform != nullptr && _platform->clock.sleepMs != nullptr) {
			_platform->clock.sleepMs(milliseconds);
			return;
		}
		busy_wait_ms(milliseconds);
	}

	static void stopPin(lexxos::platform::GpioPin pin) {
		if (!pin.isValid()) {
			return;
		}
		auto const gpio = picoGpio(pin);
		gpio_set_function(gpio, GPIO_FUNC_SIO);
		gpio_set_dir(gpio, GPIO_OUT);
		gpio_put(gpio, 0);
	}

	void stopConfiguredPins() const {
		stopPin(_config.left);
		stopPin(_config.right);
	}

	void stopMixOutput() {
		if (!_mixConfigured) {
			return;
		}
		PwmPin pins[2]{};
		auto const count = collectPins(lexxos::devices::AudioChannel::Stereo, pins);
		for (std::uint32_t index = 0; index < count; ++index) {
			pwm_set_gpio_level(pins[index].gpio, 0);
		}
		for (std::uint32_t index = 0; index < count; ++index) {
			if (!hasSlice(pins, count, index, pins[index].slice)) {
				pwm_set_enabled(pins[index].slice, false);
			}
		}
		stopConfiguredPins();
		_mixLeftLevel = mixPwmCenter;
		_mixRightLevel = mixPwmCenter;
		_mixConfigured = false;
	}

	void settleMixOutput(std::uint32_t milliseconds) {
		if (!_mixConfigured) {
			sleepMs(milliseconds);
			return;
		}
		PwmPin pins[2]{};
		auto const count = collectPins(_mixChannel, pins);
		if (count == 0) {
			stopMixOutput();
			sleepMs(milliseconds);
			return;
		}

		auto samples = (static_cast<std::uint64_t>(milliseconds) * mixSampleRateHz) / 1000ULL;
		if (samples == 0) {
			samples = 1;
		}
		auto const startLeft = _mixLeftLevel;
		auto const startRight = _mixRightLevel;
		auto nextSampleTime = get_absolute_time();
		for (std::uint64_t sample = 0; sample < samples; ++sample) {
			auto const step = static_cast<std::int32_t>(sample + 1U);
			auto const total = static_cast<std::int32_t>(samples);
			auto const left = static_cast<std::int32_t>(startLeft) + ((static_cast<std::int32_t>(mixPwmCenter) - static_cast<std::int32_t>(startLeft)) * step) / total;
			auto const right = static_cast<std::int32_t>(startRight) + ((static_cast<std::int32_t>(mixPwmCenter) - static_cast<std::int32_t>(startRight)) * step) / total;
			_mixLeftLevel = static_cast<std::uint16_t>(left);
			_mixRightLevel = static_cast<std::uint16_t>(right);
			for (std::uint32_t index = 0; index < count; ++index) {
				auto const level = pins[index].pin.number == _config.right.number ? _mixRightLevel : _mixLeftLevel;
				pwm_set_gpio_level(pins[index].gpio, level);
			}
			nextSampleTime = delayed_by_us(nextSampleTime, mixSamplePeriodUs);
			busy_wait_until(nextSampleTime);
		}
		stopMixOutput();
	}

	void appendPin(PwmPin* pins, std::uint32_t& count, lexxos::platform::GpioPin pin) const {
		if (!pin.isValid() || pins == nullptr || count >= 2) {
			return;
		}
		auto& entry = pins[count++];
		entry.pin = pin;
		entry.gpio = picoGpio(pin);
		entry.slice = pwm_gpio_to_slice_num(entry.gpio);
		entry.valid = true;
	}

	std::uint32_t collectPins(AudioChannel channel, PwmPin* pins) const {
		std::uint32_t count = 0;
		switch (channel) {
			case AudioChannel::Left:
				appendPin(pins, count, _config.left);
				break;
			case AudioChannel::Right:
				appendPin(pins, count, _config.right);
				break;
			case AudioChannel::Stereo:
				appendPin(pins, count, _config.left);
				appendPin(pins, count, _config.right);
				break;
		}
		return count;
	}

	static bool hasSlice(PwmPin const* pins, std::uint32_t count, std::uint32_t until, uint slice) {
		for (std::uint32_t index = 0; index < until && index < count; ++index) {
			if (pins[index].valid && pins[index].slice == slice) {
				return true;
			}
		}
		return false;
	}

	static float pwmDividerFor(std::uint32_t frequencyHz) {
		float divider = static_cast<float>(clock_get_hz(clk_sys)) / (static_cast<float>(frequencyHz) * static_cast<float>(pwmWrap + 1));
		if (divider < 1.0F) {
			divider = 1.0F;
		} else if (divider > 255.0F) {
			divider = 255.0F;
		}
		return divider;
	}

	static std::uint32_t phaseStepFor(std::uint32_t frequencyHz) {
		return static_cast<std::uint32_t>((static_cast<std::uint64_t>(frequencyHz) << 32U) / mixSampleRateHz);
	}

	static std::uint16_t fallbackVoiceTag(AudioToneVoice const& voice, std::size_t index) {
		auto tag = voice.tag;
		if (tag == 0) {
			tag = static_cast<std::uint16_t>(0x8000U | ((static_cast<std::uint32_t>(voice.frequencyHz) + (static_cast<std::uint32_t>(index) * 257U)) & 0x7FFFU));
		}
		return tag;
	}

	MixPhaseSlot* phaseSlotFor(std::uint16_t tag, AudioToneWaveform waveform, std::uint32_t frequencyHz) {
		MixPhaseSlot* freeSlot = nullptr;
		for (auto& slot : _mixPhaseSlots) {
			if (slot.valid && slot.tag == tag && slot.waveform == waveform && slot.frequencyHz == frequencyHz) {
				return &slot;
			}
			if (!slot.valid && freeSlot == nullptr) {
				freeSlot = &slot;
			}
		}
		if (freeSlot == nullptr) {
			freeSlot = &_mixPhaseSlots[_nextMixPhaseSlot++ % maxMixPhaseSlots];
		}
		*freeSlot = MixPhaseSlot{
		    .frequencyHz = frequencyHz,
		    .phase = 0,
		    .noise = 0x12345678U ^ (static_cast<std::uint32_t>(frequencyHz) << 8U) ^ tag,
		    .tag = tag,
		    .waveform = waveform,
		    .valid = true,
		};
		return freeSlot;
	}

	static std::int16_t waveformSample(AudioToneWaveform waveform, std::uint32_t phase, std::uint32_t& noise) {
		static constexpr std::int16_t sineTable[32] = {
		    0,
		    50,
		    98,
		    142,
		    181,
		    212,
		    236,
		    250,
		    255,
		    250,
		    236,
		    212,
		    181,
		    142,
		    98,
		    50,
		    0,
		    -50,
		    -98,
		    -142,
		    -181,
		    -212,
		    -236,
		    -250,
		    -255,
		    -250,
		    -236,
		    -212,
		    -181,
		    -142,
		    -98,
		    -50,
		};
		auto const top = static_cast<std::uint8_t>(phase >> 24U);
		switch (waveform) {
			case AudioToneWaveform::Sine:
				return sineTable[(phase >> 27U) & 0x1FU];
			case AudioToneWaveform::Triangle:
				return top < 128U
				    ? static_cast<std::int16_t>((static_cast<std::int32_t>(top) * 4) - 255)
				    : static_cast<std::int16_t>(765 - (static_cast<std::int32_t>(top) * 4));
			case AudioToneWaveform::Saw:
				return static_cast<std::int16_t>((static_cast<std::int32_t>(top) * 2) - 255);
			case AudioToneWaveform::Square:
				return (phase & 0x80000000U) != 0 ? 255 : -255;
			case AudioToneWaveform::Pulse:
				return top < 64U ? 255 : -255;
			case AudioToneWaveform::Noise:
				noise ^= noise << 13U;
				noise ^= noise >> 17U;
				noise ^= noise << 5U;
				return (noise & 0x80000000U) != 0 ? 255 : -255;
		}
		return 0;
	}

	static void panGains(std::uint8_t pan, std::int32_t& left, std::int32_t& right) {
		if (pan > 127U) {
			pan = 127U;
		}
		if (pan <= 64U) {
			left = 255;
			right = static_cast<std::int32_t>(pan) * 255 / 64;
			return;
		}
		left = static_cast<std::int32_t>(127U - pan) * 255 / 63;
		right = 255;
	}

	static std::uint16_t mixLevel(std::int32_t sum, std::uint8_t masterVolume, std::size_t voiceCount) {
		auto const divisor = voiceCount == 0 ? 1 : static_cast<std::int32_t>(voiceCount);
		auto mixed = (sum * static_cast<std::int32_t>(masterVolume)) / (255 * divisor);
		if (mixed < -255) {
			mixed = -255;
		} else if (mixed > 255) {
			mixed = 255;
		}
		auto level = static_cast<std::int32_t>(mixPwmCenter) + (mixed / 2);
		if (level < 0) {
			level = 0;
		} else if (level > mixPwmWrap) {
			level = mixPwmWrap;
		}
		return static_cast<std::uint16_t>(level);
	}

	static std::uint16_t smoothLevel(std::uint16_t target, std::uint16_t& state) {
		auto const delta = static_cast<std::int32_t>(target) - static_cast<std::int32_t>(state);
		auto step = delta / 4;
		if (step == 0 && delta != 0) {
			step = delta > 0 ? 1 : -1;
		}
		state = static_cast<std::uint16_t>(static_cast<std::int32_t>(state) + step);
		return state;
	}

	static std::uint8_t envelopeGain(std::uint64_t sample, std::uint64_t samples) {
		auto rampSamples = (mixSampleRateHz * mixRampMs) / 1000U;
		if (rampSamples == 0) {
			return 255;
		}
		if (static_cast<std::uint64_t>(rampSamples) > samples / 2U) {
			rampSamples = static_cast<std::uint32_t>(samples / 2U);
		}
		if (rampSamples == 0) {
			return 255;
		}
		auto gain = 255U;
		if (sample < rampSamples) {
			gain = static_cast<unsigned>((sample * 255ULL) / rampSamples);
		}
		auto const remaining = samples - sample - 1U;
		if (remaining < rampSamples) {
			auto const releaseGain = static_cast<unsigned>((remaining * 255ULL) / rampSamples);
			if (releaseGain < gain) {
				gain = releaseGain;
			}
		}
		return static_cast<std::uint8_t>(gain);
	}

	bool playToneOnPico(AudioToneRequest const& request) {
		stopMixOutput();
		PwmPin pins[2]{};
		auto const count = collectPins(request.channel, pins);
		if (count == 0) {
			return false;
		}

		auto pwmConfig = pwm_get_default_config();
		pwm_config_set_clkdiv(&pwmConfig, pwmDividerFor(request.frequencyHz));
		pwm_config_set_wrap(&pwmConfig, pwmWrap);

		for (std::uint32_t index = 0; index < count; ++index) {
			gpio_set_function(pins[index].gpio, GPIO_FUNC_PWM);
			if (!hasSlice(pins, count, index, pins[index].slice)) {
				pwm_init(pins[index].slice, &pwmConfig, false);
			}
		}

		auto const level = static_cast<std::uint16_t>((static_cast<std::uint32_t>(pwmWrap) * request.volume) / 255U);
		for (std::uint32_t index = 0; index < count; ++index) {
			pwm_set_gpio_level(pins[index].gpio, level);
		}
		for (std::uint32_t index = 0; index < count; ++index) {
			if (!hasSlice(pins, count, index, pins[index].slice)) {
				pwm_set_enabled(pins[index].slice, true);
			}
		}

		sleepMs(request.durationMs);

		for (std::uint32_t index = 0; index < count; ++index) {
			pwm_set_gpio_level(pins[index].gpio, 0);
		}
		for (std::uint32_t index = 0; index < count; ++index) {
			if (!hasSlice(pins, count, index, pins[index].slice)) {
				pwm_set_enabled(pins[index].slice, false);
			}
		}
		for (std::uint32_t index = 0; index < count; ++index) {
			stopPin(pins[index].pin);
		}
		return true;
	}

	bool playToneMixOnPico(AudioToneMixRequest const& request) {
		PwmPin pins[2]{};
		auto const pinCount = collectPins(request.channel, pins);
		if (pinCount == 0) {
			return false;
		}

		MixVoice voices[maxMixVoices]{};
		std::size_t voiceCount = 0;
		for (std::size_t index = 0; index < request.voiceCount && index < maxMixVoices; ++index) {
			auto const& source = request.voices[index];
			if (source.volume == 0 || source.frequencyHz < minFrequencyHz || source.frequencyHz > maxFrequencyHz) {
				continue;
			}
			auto& target = voices[voiceCount];
			target.frequencyHz = source.frequencyHz;
			target.phaseStep = phaseStepFor(source.frequencyHz);
			target.tag = fallbackVoiceTag(source, index);
			target.phaseSlot = phaseSlotFor(target.tag, source.waveform, source.frequencyHz);
			target.phase = target.phaseSlot != nullptr ? target.phaseSlot->phase : 0;
			target.noise = target.phaseSlot != nullptr ? target.phaseSlot->noise : (0x12345678U ^ (static_cast<std::uint32_t>(source.frequencyHz) << 8U) ^ target.tag);
			target.volume = source.volume;
			target.pan = source.pan;
			target.waveform = source.waveform;
			++voiceCount;
		}

		if (voiceCount == 0) {
			settleMixOutput(request.durationMs);
			return true;
		}

		auto pwmConfig = pwm_get_default_config();
		pwm_config_set_clkdiv(&pwmConfig, 1.0F);
		pwm_config_set_wrap(&pwmConfig, mixPwmWrap);

		if (!_mixConfigured || _mixChannel != request.channel) {
			stopMixOutput();
			for (std::uint32_t index = 0; index < pinCount; ++index) {
				gpio_set_function(pins[index].gpio, GPIO_FUNC_PWM);
				if (!hasSlice(pins, pinCount, index, pins[index].slice)) {
					pwm_init(pins[index].slice, &pwmConfig, false);
				}
			}
			for (std::uint32_t index = 0; index < pinCount; ++index) {
				pwm_set_gpio_level(pins[index].gpio, mixPwmCenter);
			}
			for (std::uint32_t index = 0; index < pinCount; ++index) {
				if (!hasSlice(pins, pinCount, index, pins[index].slice)) {
					pwm_set_enabled(pins[index].slice, true);
				}
			}
			_mixConfigured = true;
			_mixChannel = request.channel;
		}

		auto samples = (static_cast<std::uint64_t>(request.durationMs) * mixSampleRateHz) / 1000ULL;
		if (samples == 0) {
			samples = 1;
		}
		auto nextSampleTime = get_absolute_time();
		auto const masterVolume = request.masterVolume == 255 ? mixDefaultMasterVolume : request.masterVolume;
		for (std::uint64_t sample = 0; sample < samples; ++sample) {
			std::int32_t leftSum = 0;
			std::int32_t rightSum = 0;
			auto const envelope = envelopeGain(sample, samples);
			for (std::size_t voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex) {
				auto& voice = voices[voiceIndex];
				voice.phase += voice.phaseStep;
				auto const raw = static_cast<std::int32_t>(waveformSample(voice.waveform, voice.phase, voice.noise));
				auto const effectiveVolume = (static_cast<std::int32_t>(voice.volume) * envelope) / 255;
				auto const scaled = (raw * effectiveVolume) / 255;
				std::int32_t leftGain = 255;
				std::int32_t rightGain = 255;
				panGains(voice.pan, leftGain, rightGain);
				leftSum += (scaled * leftGain) / 255;
				rightSum += (scaled * rightGain) / 255;
			}

			auto const leftLevel = smoothLevel(mixLevel(leftSum, masterVolume, voiceCount), _mixLeftLevel);
			auto const rightLevel = smoothLevel(mixLevel(rightSum, masterVolume, voiceCount), _mixRightLevel);
			for (std::uint32_t index = 0; index < pinCount; ++index) {
				auto const level = pins[index].pin.number == _config.right.number ? rightLevel : leftLevel;
				pwm_set_gpio_level(pins[index].gpio, level);
			}
			nextSampleTime = delayed_by_us(nextSampleTime, mixSamplePeriodUs);
			busy_wait_until(nextSampleTime);
		}

		for (std::size_t voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex) {
			auto& voice = voices[voiceIndex];
			if (voice.phaseSlot != nullptr) {
				voice.phaseSlot->phase = voice.phase;
				voice.phaseSlot->noise = voice.noise;
			}
		}
		return true;
	}
#else
	void stopConfiguredPins() const {
	}
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
