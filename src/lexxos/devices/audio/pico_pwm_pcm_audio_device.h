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
	PicoPwmPcmAudioDevice(lexxos::platform::AudioBeepPins config, lexxos::platform::PlatformServices const& platform)
	    : _toneDevice(config, platform)
	    , _config(config)
	    , _platform(&platform) {
	}

	[[nodiscard]] std::string_view name() const override {
		return "picocalc-audio";
	}

	[[nodiscard]] bool initialize() {
		_ready = _toneDevice.initialize();
#if defined(LEXXOS_PLATFORM_PICO)
		if (!_pcmLockInitialized) {
			critical_section_init(&_pcmLock);
			_pcmLockInitialized = true;
		}
		resetPcmQueue();
		resetPcmOutputStats();
		invalidatePcmConfiguration();
#endif
		return _ready;
	}

	[[nodiscard]] bool isReady() const override {
		return _ready;
	}

	[[nodiscard]] bool supportsTone() const override {
		return _toneDevice.supportsTone();
	}

	[[nodiscard]] bool supportsToneMix() const override {
		return _toneDevice.supportsToneMix();
	}

	[[nodiscard]] bool supportsPcmBuffer() const override {
		return true;
	}

	[[nodiscard]] std::size_t maxToneMixVoices() const override {
		return _toneDevice.maxToneMixVoices();
	}

	[[nodiscard]] bool supportsChannel(AudioChannel channel) const override {
		return _toneDevice.supportsChannel(channel);
	}

	[[nodiscard]] std::uint32_t minToneFrequencyHz() const override {
		return _toneDevice.minToneFrequencyHz();
	}

	[[nodiscard]] std::uint32_t maxToneFrequencyHz() const override {
		return _toneDevice.maxToneFrequencyHz();
	}

	[[nodiscard]] std::uint32_t defaultToneFrequencyHz() const override {
		return _toneDevice.defaultToneFrequencyHz();
	}

	[[nodiscard]] std::uint32_t maxToneDurationMs() const override {
		return _toneDevice.maxToneDurationMs();
	}

	[[nodiscard]] std::uint32_t defaultToneDurationMs() const override {
		return _toneDevice.defaultToneDurationMs();
	}

	bool playTone(AudioToneRequest const& request) override {
#if defined(LEXXOS_PLATFORM_PICO)
		stopPcmOutput();
		invalidatePcmConfiguration();
#endif
		auto const played = _toneDevice.playTone(request);
#if defined(LEXXOS_PLATFORM_PICO)
		// Tone playback reconfigures the same PWM pins/slices. Force the next PCM
		// stream to restore its own PWM-DAC configuration from scratch.
		invalidatePcmConfiguration();
#endif
		return played;
	}

	bool playToneMix(AudioToneMixRequest const& request) override {
#if defined(LEXXOS_PLATFORM_PICO)
		stopPcmOutput();
		invalidatePcmConfiguration();
#endif
		auto const played = _toneDevice.playToneMix(request);
#if defined(LEXXOS_PLATFORM_PICO)
		invalidatePcmConfiguration();
#endif
		return played;
	}

	bool playPcmBuffer(AudioPcmBufferRequest const& request) override {
		if (!_ready || !supportsChannel(request.channel)) {
			return false;
		}
		if (request.frames == nullptr || request.frameCount == 0) {
			return false;
		}
		if (request.sampleRateHz < minPcmSampleRateHz || request.sampleRateHz > maxPcmSampleRateHz) {
			return false;
		}
#if defined(LEXXOS_PLATFORM_PICO)
		return playPcmBufferOnPico(request);
#else
		(void) request;
		return false;
#endif
	}

	bool drainPcmOutput(std::uint32_t timeoutMs) override {
#if defined(LEXXOS_PLATFORM_PICO)
		if (!_pcmLockInitialized) {
			return true;
		}
		auto const deadline = make_timeout_time_ms(timeoutMs);
		while (true) {
			if (queuedFrames() == 0) {
				stopPcmOutput();
				return true;
			}
			if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
				stopPcmOutput();
				return false;
			}
			sleep_us(1000);
		}
#else
		(void) timeoutMs;
		return true;
#endif
	}

	void stopPcmOutput() override {
#if defined(LEXXOS_PLATFORM_PICO)
		stopPcmTimer();
		resetPcmQueue();
		setPcmPinsToCenter();
		invalidatePcmConfiguration();
#endif
	}

	void resetPcmOutputStats() override {
#if defined(LEXXOS_PLATFORM_PICO)
		if (!_pcmLockInitialized) {
			_pcmStats = AudioPcmOutputStats{};
			_pcmStats.ringCapacity = pcmRingCapacity;
			return;
		}
		critical_section_enter_blocking(&_pcmLock);
		_pcmStats = AudioPcmOutputStats{};
		_pcmStats.ringCapacity = pcmRingCapacity;
		_pcmStats.ringFill = static_cast<std::uint32_t>(_pcmFill);
		critical_section_exit(&_pcmLock);
#endif
	}

	[[nodiscard]] bool pcmOutputStats(AudioPcmOutputStats& stats) const override {
#if defined(LEXXOS_PLATFORM_PICO)
		if (!_pcmLockInitialized) {
			stats = _pcmStats;
			return true;
		}
		critical_section_enter_blocking(&_pcmLock);
		stats = _pcmStats;
		stats.ringCapacity = pcmRingCapacity;
		stats.ringFill = static_cast<std::uint32_t>(_pcmFill);
		critical_section_exit(&_pcmLock);
		return true;
#else
		stats = AudioPcmOutputStats{};
		return false;
#endif
	}

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

	[[nodiscard]] static uint picoGpio(lexxos::platform::GpioPin pin) {
		return static_cast<uint>(pin.number);
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

	static std::uint16_t pcmToPwmLevel(std::int16_t sample) {
		auto const shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(sample) + 32768);
		return static_cast<std::uint16_t>((shifted * pcmPwmWrap) / 65535U);
	}

	void invalidatePcmConfiguration() {
		_pcmConfigured = false;
		_pcmSampleRateHz = 0;
		_pcmTimerSampleRateHz = 0;
		_pcmPinCount = 0;
		for (auto& pin : _pcmPins) {
			pin = PwmPin{};
		}
	}

	void configurePcmOutput(AudioChannel channel, std::uint32_t sampleRateHz, PwmPin const* pins, std::uint32_t pinCount) {
		if (_pcmConfigured && _pcmChannel == channel && _pcmSampleRateHz == sampleRateHz) {
			return;
		}

		stopPcmTimer();
		resetPcmQueue();
		_pcmConfigured = false;
		_pcmPinCount = pinCount;
		for (std::uint32_t index = 0; index < pinCount; ++index) {
			_pcmPins[index] = pins[index];
		}

		auto pwmConfig = pwm_get_default_config();
		pwm_config_set_clkdiv(&pwmConfig, 1.0F);
		pwm_config_set_wrap(&pwmConfig, pcmPwmWrap);
		for (std::uint32_t index = 0; index < pinCount; ++index) {
			gpio_set_function(_pcmPins[index].gpio, GPIO_FUNC_PWM);
			if (!hasSlice(_pcmPins, pinCount, index, _pcmPins[index].slice)) {
				pwm_init(_pcmPins[index].slice, &pwmConfig, false);
			}
		}
		setPcmPinsToCenter();
		for (std::uint32_t index = 0; index < pinCount; ++index) {
			if (!hasSlice(_pcmPins, pinCount, index, _pcmPins[index].slice)) {
				pwm_set_enabled(_pcmPins[index].slice, true);
			}
		}
		_pcmConfigured = true;
		_pcmChannel = channel;
		_pcmSampleRateHz = sampleRateHz;
		_pcmStats.sampleRateHz = sampleRateHz;
	}

	void setPcmPinsToCenter() {
		for (std::uint32_t index = 0; index < _pcmPinCount; ++index) {
			if (_pcmPins[index].valid) {
				pwm_set_gpio_level(_pcmPins[index].gpio, pcmPwmCenter);
			}
		}
	}

	void resetPcmQueue() {
		if (_pcmLockInitialized) {
			critical_section_enter_blocking(&_pcmLock);
		}
		_pcmReadIndex = 0;
		_pcmWriteIndex = 0;
		_pcmFill = 0;
		if (_pcmLockInitialized) {
			critical_section_exit(&_pcmLock);
		}
	}

	[[nodiscard]] std::size_t queuedFrames() const {
		if (!_pcmLockInitialized) {
			return _pcmFill;
		}
		critical_section_enter_blocking(&_pcmLock);
		auto const fill = _pcmFill;
		critical_section_exit(&_pcmLock);
		return fill;
	}

	[[nodiscard]] bool enqueueFrame(AudioPcmFrame const& frame) {
		bool queued = false;
		critical_section_enter_blocking(&_pcmLock);
		if (_pcmFill < pcmRingCapacity) {
			_pcmRing[_pcmWriteIndex] = frame;
			_pcmWriteIndex = (_pcmWriteIndex + 1U) % pcmRingCapacity;
			++_pcmFill;
			++_pcmStats.framesQueued;
			_pcmStats.ringFill = static_cast<std::uint32_t>(_pcmFill);
			if (_pcmStats.ringFill > _pcmStats.ringHighWatermark) {
				_pcmStats.ringHighWatermark = _pcmStats.ringFill;
			}
			queued = true;
		}
		critical_section_exit(&_pcmLock);
		return queued;
	}

	[[nodiscard]] bool dequeueFrame(AudioPcmFrame& frame) {
		bool hasFrame = false;
		critical_section_enter_blocking(&_pcmLock);
		if (_pcmFill > 0) {
			frame = _pcmRing[_pcmReadIndex];
			_pcmReadIndex = (_pcmReadIndex + 1U) % pcmRingCapacity;
			--_pcmFill;
			_pcmStats.ringFill = static_cast<std::uint32_t>(_pcmFill);
			hasFrame = true;
		}
		critical_section_exit(&_pcmLock);
		return hasFrame;
	}

	void recordFullWait() {
		critical_section_enter_blocking(&_pcmLock);
		++_pcmStats.fullWaits;
		critical_section_exit(&_pcmLock);
	}

	void recordUnderrun() {
		critical_section_enter_blocking(&_pcmLock);
		++_pcmStats.underruns;
		critical_section_exit(&_pcmLock);
	}

	void recordWrittenFrame() {
		critical_section_enter_blocking(&_pcmLock);
		++_pcmStats.framesWritten;
		critical_section_exit(&_pcmLock);
	}

	static bool pcmTimerCallback(repeating_timer_t* timer) {
		auto* self = static_cast<PicoPwmPcmAudioDevice*>(timer->user_data);
		if (self == nullptr) {
			return false;
		}
		self->writeNextPcmFrameFromTimer();
		return true;
	}

	void writeNextPcmFrameFromTimer() {
		AudioPcmFrame frame{};
		if (!dequeueFrame(frame)) {
			recordUnderrun();
		}

		for (std::uint32_t pinIndex = 0; pinIndex < _pcmPinCount; ++pinIndex) {
			std::int16_t sample = 0;
			if (_pcmChannel == AudioChannel::Left) {
				sample = frame.left;
			} else if (_pcmChannel == AudioChannel::Right) {
				sample = frame.right;
			} else {
				sample = _pcmPins[pinIndex].pin.number == _config.right.number ? frame.right : frame.left;
			}
			pwm_set_gpio_level(_pcmPins[pinIndex].gpio, pcmToPwmLevel(sample));
		}
		recordWrittenFrame();
	}

	bool startPcmTimer(std::uint32_t sampleRateHz) {
		if (_pcmTimerRunning && _pcmTimerSampleRateHz == sampleRateHz) {
			return true;
		}
		stopPcmTimer();
		auto const periodUs = sampleRateHz == 0 ? 1U : 1000000U / sampleRateHz;
		auto const safePeriodUs = periodUs == 0 ? 1U : periodUs;
		_pcmTimerSampleRateHz = sampleRateHz;
		_pcmTimerRunning = add_repeating_timer_us(
		    -static_cast<std::int64_t>(safePeriodUs),
		    &PicoPwmPcmAudioDevice::pcmTimerCallback,
		    this,
		    &_pcmTimer);
		return _pcmTimerRunning;
	}

	void stopPcmTimer() {
		if (_pcmTimerRunning) {
			cancel_repeating_timer(&_pcmTimer);
			_pcmTimerRunning = false;
			_pcmTimerSampleRateHz = 0;
		}
	}

	bool playPcmBufferOnPico(AudioPcmBufferRequest const& request) {
		PwmPin pins[2]{};
		auto const pinCount = collectPins(request.channel, pins);
		if (pinCount == 0) {
			return false;
		}

		configurePcmOutput(request.channel, request.sampleRateHz, pins, pinCount);
		critical_section_enter_blocking(&_pcmLock);
		_pcmStats.sampleRateHz = request.sampleRateHz;
		++_pcmStats.bufferCalls;
		critical_section_exit(&_pcmLock);

		bool startPending = !_pcmTimerRunning;
		for (std::size_t frameIndex = 0; frameIndex < request.frameCount; ++frameIndex) {
			while (!enqueueFrame(request.frames[frameIndex])) {
				if (startPending) {
					if (!startPcmTimer(request.sampleRateHz)) {
						return false;
					}
					startPending = false;
				}
				recordFullWait();
				sleep_us(250);
			}
			if (startPending && queuedFrames() >= pcmRingStartThreshold) {
				if (!startPcmTimer(request.sampleRateHz)) {
					return false;
				}
				startPending = false;
			}
		}
		if (startPending) {
			return startPcmTimer(request.sampleRateHz);
		}
		return true;
	}
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
