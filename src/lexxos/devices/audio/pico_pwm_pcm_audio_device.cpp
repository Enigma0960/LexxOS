#include <lexxos/devices/audio/pico_pwm_pcm_audio_device.h>

namespace lexxos::devices {

PicoPwmPcmAudioDevice::PicoPwmPcmAudioDevice(lexxos::platform::AudioBeepPins config, lexxos::platform::PlatformServices const& platform)
    : _toneDevice(config, platform)
    , _config(config)
    , _platform(&platform) {
}

std::string_view PicoPwmPcmAudioDevice::name() const {
	return "picocalc-audio";
}

bool PicoPwmPcmAudioDevice::initialize() {
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

bool PicoPwmPcmAudioDevice::isReady() const {
	return _ready;
}

bool PicoPwmPcmAudioDevice::supportsTone() const {
	return _toneDevice.supportsTone();
}

bool PicoPwmPcmAudioDevice::supportsToneMix() const {
	return _toneDevice.supportsToneMix();
}

bool PicoPwmPcmAudioDevice::supportsPcmBuffer() const {
	return true;
}

std::size_t PicoPwmPcmAudioDevice::maxToneMixVoices() const {
	return _toneDevice.maxToneMixVoices();
}

bool PicoPwmPcmAudioDevice::supportsChannel(AudioChannel channel) const {
	return _toneDevice.supportsChannel(channel);
}

std::uint32_t PicoPwmPcmAudioDevice::minToneFrequencyHz() const {
	return _toneDevice.minToneFrequencyHz();
}

std::uint32_t PicoPwmPcmAudioDevice::maxToneFrequencyHz() const {
	return _toneDevice.maxToneFrequencyHz();
}

std::uint32_t PicoPwmPcmAudioDevice::defaultToneFrequencyHz() const {
	return _toneDevice.defaultToneFrequencyHz();
}

std::uint32_t PicoPwmPcmAudioDevice::maxToneDurationMs() const {
	return _toneDevice.maxToneDurationMs();
}

std::uint32_t PicoPwmPcmAudioDevice::defaultToneDurationMs() const {
	return _toneDevice.defaultToneDurationMs();
}

bool PicoPwmPcmAudioDevice::playTone(AudioToneRequest const& request) {
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

bool PicoPwmPcmAudioDevice::playToneMix(AudioToneMixRequest const& request) {
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

bool PicoPwmPcmAudioDevice::playPcmBuffer(AudioPcmBufferRequest const& request) {
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

bool PicoPwmPcmAudioDevice::drainPcmOutput(std::uint32_t timeoutMs) {
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

void PicoPwmPcmAudioDevice::stopPcmOutput() {
#if defined(LEXXOS_PLATFORM_PICO)
	stopPcmTimer();
	resetPcmQueue();
	setPcmPinsToCenter();
	invalidatePcmConfiguration();
#endif
}

void PicoPwmPcmAudioDevice::resetPcmOutputStats() {
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

bool PicoPwmPcmAudioDevice::pcmOutputStats(AudioPcmOutputStats& stats) const {
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

#if defined(LEXXOS_PLATFORM_PICO)
uint PicoPwmPcmAudioDevice::picoGpio(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::stopPin(lexxos::platform::GpioPin pin) {
	if (!pin.isValid()) {
		return;
	}

	auto const gpio = picoGpio(pin);
	gpio_set_function(gpio, GPIO_FUNC_SIO);
	gpio_set_dir(gpio, GPIO_OUT);
	gpio_put(gpio, 0);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::appendPin(PwmPin* pins, std::uint32_t& count, lexxos::platform::GpioPin pin) const {
	if (!pin.isValid() || pins == nullptr || count >= 2) {
		return;
	}

	auto& entry = pins[count++];
	entry.pin = pin;
	entry.gpio = picoGpio(pin);
	entry.slice = pwm_gpio_to_slice_num(entry.gpio);
	entry.valid = true;
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
std::uint32_t PicoPwmPcmAudioDevice::collectPins(AudioChannel channel, PwmPin* pins) const {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::hasSlice(PwmPin const* pins, std::uint32_t count, std::uint32_t until, uint slice) {
	for (std::uint32_t index = 0; index < until && index < count; ++index) {
		if (pins[index].valid && pins[index].slice == slice) {
			return true;
		}
	}

	return false;
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
std::uint16_t PicoPwmPcmAudioDevice::pcmToPwmLevel(std::int16_t sample) {
	auto const shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(sample) + 32768);
	return static_cast<std::uint16_t>((shifted * pcmPwmWrap) / 65535U);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::invalidatePcmConfiguration() {
	_pcmConfigured = false;
	_pcmSampleRateHz = 0;
	_pcmTimerSampleRateHz = 0;
	_pcmPinCount = 0;
	for (auto& pin : _pcmPins) {
		pin = PwmPin{};
	}
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::configurePcmOutput(AudioChannel channel, std::uint32_t sampleRateHz, PwmPin const* pins, std::uint32_t pinCount) {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::setPcmPinsToCenter() {
	for (std::uint32_t index = 0; index < _pcmPinCount; ++index) {
		if (_pcmPins[index].valid) {
			pwm_set_gpio_level(_pcmPins[index].gpio, pcmPwmCenter);
		}
	}
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::resetPcmQueue() {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
std::size_t PicoPwmPcmAudioDevice::queuedFrames() const {
	if (!_pcmLockInitialized) {
		return _pcmFill;
	}

	critical_section_enter_blocking(&_pcmLock);
	auto const fill = _pcmFill;
	critical_section_exit(&_pcmLock);
	return fill;
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::enqueueFrame(AudioPcmFrame const& frame) {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::dequeueFrame(AudioPcmFrame& frame) {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::recordFullWait() {
	critical_section_enter_blocking(&_pcmLock);
	++_pcmStats.fullWaits;
	critical_section_exit(&_pcmLock);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::recordUnderrun() {
	critical_section_enter_blocking(&_pcmLock);
	++_pcmStats.underruns;
	critical_section_exit(&_pcmLock);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::recordWrittenFrame() {
	critical_section_enter_blocking(&_pcmLock);
	++_pcmStats.framesWritten;
	critical_section_exit(&_pcmLock);
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::pcmTimerCallback(repeating_timer_t* timer) {
	auto* self = static_cast<PicoPwmPcmAudioDevice*>(timer->user_data);
	if (self == nullptr) {
		return false;
	}

	self->writeNextPcmFrameFromTimer();
	return true;
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::writeNextPcmFrameFromTimer() {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::startPcmTimer(std::uint32_t sampleRateHz) {
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
#endif

#if defined(LEXXOS_PLATFORM_PICO)
void PicoPwmPcmAudioDevice::stopPcmTimer() {
	if (_pcmTimerRunning) {
		cancel_repeating_timer(&_pcmTimer);
		_pcmTimerRunning = false;
		_pcmTimerSampleRateHz = 0;
	}
}
#endif

#if defined(LEXXOS_PLATFORM_PICO)
bool PicoPwmPcmAudioDevice::playPcmBufferOnPico(AudioPcmBufferRequest const& request) {
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

} // namespace lexxos::devices
