#include <lexxos/devices/audio/espidf_es8311_audio_device.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <esp_err.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint32_t minFrequencyHz = 20;
	constexpr std::uint32_t maxFrequencyHz = 20000;
	constexpr std::uint32_t defaultFrequencyHz = 880;
	constexpr std::uint32_t defaultDurationMs = 120;
	constexpr std::uint32_t maxDurationMs = 5000;
	constexpr std::uint32_t minPcmSampleRateHz = 8000;
	constexpr std::uint32_t maxPcmSampleRateHz = 48000;
	constexpr std::uint32_t defaultNativeSampleRateHz = 48000;
	constexpr std::uint8_t es8311DacVolumeMinus6Db = 0xB3;
	constexpr std::uint8_t es8311DacMute = 0x60;
	constexpr std::uint8_t es8311DacUnmute = 0x00;
	constexpr std::uint64_t resampleOneQ32 = 1ULL << 32U;
	constexpr std::uint32_t toneFadeMs = 5;
	constexpr std::uint32_t outputPrimeMs = 10;
	constexpr std::uint32_t silenceTailMs = 20;
	constexpr std::size_t framesPerChunk = 256;
	constexpr std::uint32_t i2sDmaFrameNum = 256;
	constexpr auto es8311MclkMultiple = I2S_MCLK_MULTIPLE_256;
	constexpr std::int16_t sineTable[32] = {
	    0,
	    6393,
	    12539,
	    18205,
	    23170,
	    27245,
	    30273,
	    32137,
	    32767,
	    32137,
	    30273,
	    27245,
	    23170,
	    18205,
	    12539,
	    6393,
	    0,
	    -6393,
	    -12539,
	    -18205,
	    -23170,
	    -27245,
	    -30273,
	    -32137,
	    -32767,
	    -32137,
	    -30273,
	    -27245,
	    -23170,
	    -18205,
	    -12539,
	    -6393,
	};

	struct CodecRegister {
		std::uint8_t reg;
		std::uint8_t value;
	};

	constexpr CodecRegister es8311OutputEnableSequence[] = {
	    {0x31,           es8311DacMute},
	    {0x0D,	                0xFA},
	    {0x44,	                0x08},
	    {0x44,	                0x08},
	    {0x01,	                0x30},
	    {0x02,	                0x00},
	    {0x03,	                0x10},
	    {0x16,	                0x24},
	    {0x04,	                0x10},
	    {0x05,	                0x00},
	    {0x0B,	                0x00},
	    {0x0C,	                0x00},
	    {0x10,	                0x1F},
	    {0x11,	                0x7F},
	    {0x00,	                0x80},
	    {0x01,	                0xBF},
	    {0x02,	                0x18},
	    {0x03,	                0x10},
	    {0x04,	                0x10},
	    {0x05,	                0x00},
	    {0x06,	                0x03},
	    {0x07,	                0x00},
	    {0x08,	                0xFF},
	    {0x09,	                0x0C},
	    {0x0A,	                0x0C},
	    {0x13,	                0x10},
	    {0x1B,	                0x0A},
	    {0x1C,	                0x6A},
	    {0x44,	                0x58},
	    {0x00,	                0x80},
	    {0x01,	                0xBF},
	    {0x09,	                0x0C},
	    {0x0A,	                0x0C},
	    {0x17,	                0xBF},
	    {0x0E,	                0x02},
	    {0x12,	                0x00},
	    {0x14,	                0x1A},
	    {0x0D,	                0x01},
	    {0x15,	                0x40},
	    {0x32, es8311DacVolumeMinus6Db},
	    {0x37,	                0x08},
	    {0x45,	                0x00},
	    {0x31,           es8311DacMute},
	};

	constexpr CodecRegister es8311InputEnableSequence[] = {
	    {0x0E, 0x02},
	    {0x0F, 0x44},
	    {0x12, 0x00},
	    {0x13, 0x10},
	    {0x14, 0x1A},
	    {0x15, 0x00},
	    {0x17, 0xD2},
	};

	int i2sPort(int index) {
		return index == 0 ? 0 : 1;
	}

	gpio_num_t i2sGpio(lexxos::platform::GpioPin pin) {
		return pin.isValid() ? static_cast<gpio_num_t>(pin.number) : I2S_GPIO_UNUSED;
	}

	gpio_num_t outputGpio(lexxos::platform::GpioPin pin) {
		return static_cast<gpio_num_t>(pin.number);
	}

	int gpioLevelFor(lexxos::platform::GpioActiveLevel activeLevel, bool active) {
		auto const activeHigh = activeLevel == lexxos::platform::GpioActiveLevel::High;
		return (active == activeHigh) ? 1 : 0;
	}

	i2s_clock_src_t i2sClockSource() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
		return I2S_CLK_SRC_PLL_240M;
#else
		return I2S_CLK_SRC_DEFAULT;
#endif
	}

	std::int16_t scaleSample(std::int16_t sample, std::uint8_t volume) {
		auto const scaled = (static_cast<std::int32_t>(sample) * static_cast<std::int32_t>(volume)) / 255;
		return static_cast<std::int16_t>(scaled);
	}

	AudioPcmFrame interpolateFrame(AudioPcmFrame const& from, AudioPcmFrame const& to, std::uint64_t phaseQ32) {
		auto const inverse = resampleOneQ32 - phaseQ32;
		return AudioPcmFrame{
		    .left = static_cast<std::int16_t>(((static_cast<std::int64_t>(from.left) * static_cast<std::int64_t>(inverse))
		                                          + (static_cast<std::int64_t>(to.left) * static_cast<std::int64_t>(phaseQ32)))
		        >> 32U),
		    .right = static_cast<std::int16_t>(((static_cast<std::int64_t>(from.right) * static_cast<std::int64_t>(inverse))
		                                           + (static_cast<std::int64_t>(to.right) * static_cast<std::int64_t>(phaseQ32)))
		        >> 32U),
		};
	}

} // namespace

EspIdfEs8311AudioDevice::EspIdfEs8311AudioDevice(
    lexxos::platform::AudioI2sCodecPins config,
    I2cBus& controlBus,
    lexxos::platform::PlatformServices const& platform)
    : _config(config)
    , _controlBus(&controlBus)
    , _platform(&platform) {
}

EspIdfEs8311AudioDevice::~EspIdfEs8311AudioDevice() {
	_releaseI2s();
}

std::string_view EspIdfEs8311AudioDevice::name() const {
	return _config.name != nullptr ? std::string_view{_config.name} : std::string_view{"espidf-es8311-audio"};
}

bool EspIdfEs8311AudioDevice::initialize() {
	if (_ready) {
		return true;
	}
	if (!_config.isValid() || _controlBus == nullptr || !_controlBus->isReady()) {
		return false;
	}
	if (!_configureAmplifier()) {
		return false;
	}
	if (!_controlBus->probe(_config.i2cAddress)) {
		return false;
	}
	if (!_initializeI2s()) {
		_releaseI2s();
		return false;
	}
	if (!_initializeCodec()) {
		_releaseI2s();
		return false;
	}
	_ready = true;
	return true;
}

bool EspIdfEs8311AudioDevice::isReady() const {
	return _ready;
}

bool EspIdfEs8311AudioDevice::supportsPcmBuffer() const {
	return _config.hasOutput();
}

std::uint32_t EspIdfEs8311AudioDevice::preferredPcmSampleRateHz() const {
	return _nativeSampleRateHz();
}

bool EspIdfEs8311AudioDevice::supportsChannel(AudioChannel channel) const {
	switch (channel) {
		case AudioChannel::Left:
		case AudioChannel::Right:
		case AudioChannel::Stereo:
			return _config.hasOutput();
	}
	return false;
}

std::uint32_t EspIdfEs8311AudioDevice::minToneFrequencyHz() const {
	return minFrequencyHz;
}

std::uint32_t EspIdfEs8311AudioDevice::maxToneFrequencyHz() const {
	auto const nativeSampleRate = _nativeSampleRateHz();
	auto const nyquistLimit = nativeSampleRate > 2U ? (nativeSampleRate / 2U) - 1U : maxFrequencyHz;
	return nyquistLimit < maxFrequencyHz ? nyquistLimit : maxFrequencyHz;
}

std::uint32_t EspIdfEs8311AudioDevice::defaultToneFrequencyHz() const {
	return _config.defaultFrequencyHz == 0 ? defaultFrequencyHz : _config.defaultFrequencyHz;
}

std::uint32_t EspIdfEs8311AudioDevice::maxToneDurationMs() const {
	return maxDurationMs;
}

std::uint32_t EspIdfEs8311AudioDevice::defaultToneDurationMs() const {
	return _config.defaultDurationMs == 0 ? defaultDurationMs : _config.defaultDurationMs;
}

bool EspIdfEs8311AudioDevice::playTone(AudioToneRequest const& request) {
	if (!_ready && !initialize()) {
		return false;
	}
	(void) _flushResampledOutput();
	_resetPcmResampler();
	if (!supportsChannel(request.channel)
	    || request.frequencyHz < minFrequencyHz
	    || request.frequencyHz > maxToneFrequencyHz()
	    || request.durationMs == 0
	    || request.durationMs > maxDurationMs
	    || _txHandle == nullptr) {
		return false;
	}

	auto const sampleRate = _nativeSampleRateHz();
	if (!_prepareOutput(sampleRate)) {
		return false;
	}
	auto framesRemaining = static_cast<std::uint32_t>((static_cast<std::uint64_t>(sampleRate) * request.durationMs) / 1000ULL);
	if (framesRemaining == 0) {
		framesRemaining = 1;
	}
	auto const totalFrames = framesRemaining;
	auto fadeFrames = static_cast<std::uint32_t>((static_cast<std::uint64_t>(sampleRate) * toneFadeMs) / 1000ULL);
	if (fadeFrames > totalFrames / 2U) {
		fadeFrames = totalFrames / 2U;
	}
	auto const phaseStep = static_cast<std::uint32_t>((static_cast<std::uint64_t>(request.frequencyHz) << 32U) / sampleRate);
	std::uint32_t phase = 0;
	std::uint32_t frameIndex = 0;
	std::array<AudioPcmFrame, framesPerChunk> frames{};

	while (framesRemaining != 0) {
		auto const chunkFrames = framesRemaining < frames.size() ? framesRemaining : static_cast<std::uint32_t>(frames.size());
		for (std::uint32_t index = 0; index < chunkFrames; ++index) {
			phase += phaseStep;
			auto sample = scaleSample(sineTable[(phase >> 27U) & 0x1FU], request.volume);
			if (fadeFrames != 0) {
				std::uint32_t gain = 255;
				if (frameIndex < fadeFrames) {
					gain = static_cast<std::uint32_t>((static_cast<std::uint64_t>(frameIndex) * 255ULL) / fadeFrames);
				}
				auto const framesToEnd = totalFrames - frameIndex - 1U;
				if (framesToEnd < fadeFrames) {
					auto const tailGain = static_cast<std::uint32_t>((static_cast<std::uint64_t>(framesToEnd) * 255ULL) / fadeFrames);
					if (tailGain < gain) {
						gain = tailGain;
					}
				}
				sample = static_cast<std::int16_t>((static_cast<std::int32_t>(sample) * static_cast<std::int32_t>(gain)) / 255);
			}
			frames[index] = {
			    .left = request.channel == AudioChannel::Right ? static_cast<std::int16_t>(0) : sample,
			    .right = request.channel == AudioChannel::Left ? static_cast<std::int16_t>(0) : sample,
			};
			++frameIndex;
		}

		if (!_writePcmFrames(frames.data(), chunkFrames, request.channel, false)) {
			return false;
		}
		framesRemaining -= chunkFrames;
	}

	auto const silenceFrames = static_cast<std::uint32_t>((static_cast<std::uint64_t>(sampleRate) * silenceTailMs) / 1000ULL);
	auto const silenceWritten = _writeSilence(silenceFrames);
	auto const muted = _setCodecMute(true);
	auto const ampOff = _setAmplifierEnabled(false);
	auto const ok = silenceWritten && muted && ampOff;
	_releaseI2s();
	return ok;
}

bool EspIdfEs8311AudioDevice::playPcmBuffer(AudioPcmBufferRequest const& request) {
	if (!_ready && !initialize()) {
		return false;
	}
	if (!supportsPcmBuffer()
	    || !supportsChannel(request.channel)
	    || request.frames == nullptr
	    || request.frameCount == 0
	    || request.sampleRateHz < minPcmSampleRateHz
	    || request.sampleRateHz > maxPcmSampleRateHz
	    || _txHandle == nullptr) {
		return false;
	}
	if (!_prepareOutput(_nativeSampleRateHz())) {
		return false;
	}
	++_pcmStats.bufferCalls;
	_pcmStats.sampleRateHz = request.sampleRateHz;
	if (request.sampleRateHz == _nativeSampleRateHz()) {
		if (!_flushResampledOutput()) {
			return false;
		}
		_resetPcmResampler();
		return _writePcmFrames(request.frames, request.frameCount, request.channel, true);
	}
	if (!_writePcmFramesResampled(request.frames, request.frameCount, request.channel, request.sampleRateHz)) {
		return false;
	}
	_pcmStats.framesQueued += request.frameCount;
	_pcmStats.framesWritten += request.frameCount;
	return true;
}

bool EspIdfEs8311AudioDevice::drainPcmOutput(std::uint32_t timeoutMs) {
	(void) timeoutMs;
	if (!_flushResampledOutput()) {
		(void) _setCodecMute(true);
		(void) _setAmplifierEnabled(false);
		_releaseI2s();
		return false;
	}
	auto const silenceWritten = _writeSilence(static_cast<std::uint32_t>((static_cast<std::uint64_t>(_nativeSampleRateHz()) * silenceTailMs) / 1000ULL));
	auto const muted = _setCodecMute(true);
	auto const ampOff = _setAmplifierEnabled(false);
	auto const ok = silenceWritten && muted && ampOff;
	_releaseI2s();
	return ok;
}

void EspIdfEs8311AudioDevice::stopPcmOutput() {
	(void) _flushResampledOutput();
	(void) _writeSilence(static_cast<std::uint32_t>((static_cast<std::uint64_t>(_nativeSampleRateHz()) * silenceTailMs) / 1000ULL));
	(void) _setCodecMute(true);
	(void) _setAmplifierEnabled(false);
	_resetPcmResampler();
	_releaseI2s();
}

void EspIdfEs8311AudioDevice::resetPcmOutputStats() {
	_pcmStats = AudioPcmOutputStats{};
	_pcmStats.sampleRateHz = _nativeSampleRateHz();
	_resetPcmResampler();
}

bool EspIdfEs8311AudioDevice::pcmOutputStats(AudioPcmOutputStats& stats) const {
	stats = _pcmStats;
	return true;
}

bool EspIdfEs8311AudioDevice::supportsPcmCapture() const {
	return _config.hasInput();
}

std::uint32_t EspIdfEs8311AudioDevice::preferredCaptureSampleRateHz() const {
	return _nativeSampleRateHz();
}

std::string_view EspIdfEs8311AudioDevice::microphoneName() const {
	return _config.microphoneName != nullptr ? std::string_view{_config.microphoneName} : std::string_view{"espidf-es8311-mic"};
}

bool EspIdfEs8311AudioDevice::capturePcm(MicrophonePcmCaptureRequest const& request) {
	if (request.framesRead != nullptr) {
		*request.framesRead = 0;
	}
	if (!_ready && !initialize()) {
		return false;
	}
	auto const sampleRate = request.sampleRateHz == 0 ? _nativeSampleRateHz() : request.sampleRateHz;
	if (!_config.hasInput()
	    || request.frames == nullptr
	    || request.frameCapacity == 0
	    || sampleRate < minPcmSampleRateHz
	    || sampleRate > maxPcmSampleRateHz
	    || _rxHandle == nullptr) {
		return false;
	}
	if (!_prepareInput() || !_ensureI2sSampleRate(sampleRate)) {
		return false;
	}

	auto* const rx = static_cast<i2s_chan_handle_t>(_rxHandle);
	std::array<AudioPcmFrame, framesPerChunk> discard{};
	std::size_t discardBytes = 0;
	while (i2s_channel_read(rx, discard.data(), sizeof(discard), &discardBytes, 0) == ESP_OK && discardBytes != 0) {
	}

	std::size_t bytesRead = 0;
	auto const requestedBytes = request.frameCapacity * sizeof(AudioPcmFrame);
	if (i2s_channel_read(rx, request.frames, requestedBytes, &bytesRead, request.timeoutMs) != ESP_OK) {
		return false;
	}
	auto const framesRead = bytesRead / sizeof(AudioPcmFrame);
	if (request.framesRead != nullptr) {
		*request.framesRead = framesRead;
	}
	return framesRead != 0;
}

bool EspIdfEs8311AudioDevice::_configureAmplifier() {
	if (!_config.amplifierEnable.isValid()) {
		_amplifierConfigured = true;
		return true;
	}
	if (_amplifierConfigured) {
		return true;
	}

	gpio_config_t config{};
	config.pin_bit_mask = 1ULL << static_cast<unsigned>(_config.amplifierEnable.number);
	config.mode = GPIO_MODE_OUTPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;
	if (gpio_config(&config) != ESP_OK) {
		return false;
	}

	_amplifierConfigured = true;
	auto const inactiveLevel = gpioLevelFor(_config.amplifierEnableLevel, false);
	if (gpio_set_level(outputGpio(_config.amplifierEnable), inactiveLevel) != ESP_OK) {
		_amplifierConfigured = false;
		return false;
	}
	_amplifierEnabled = false;
	return true;
}

bool EspIdfEs8311AudioDevice::_setAmplifierEnabled(bool enabled) {
	if (!_config.amplifierEnable.isValid()) {
		_amplifierEnabled = false;
		return true;
	}
	if (!_amplifierConfigured && !_configureAmplifier()) {
		return false;
	}
	auto const level = gpioLevelFor(_config.amplifierEnableLevel, enabled);
	if (gpio_set_level(outputGpio(_config.amplifierEnable), level) != ESP_OK) {
		return false;
	}
	_amplifierEnabled = enabled;
	return true;
}

bool EspIdfEs8311AudioDevice::_initializeI2s() {
	i2s_chan_handle_t tx = nullptr;
	i2s_chan_handle_t rx = nullptr;
	auto const sampleRate = _nativeSampleRateHz();
	i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(i2sPort(_config.i2sIndex), I2S_ROLE_MASTER);
	channelConfig.dma_desc_num = 8;
	channelConfig.dma_frame_num = i2sDmaFrameNum;
	channelConfig.auto_clear = true;

	auto* txOut = _config.hasOutput() ? &tx : nullptr;
	auto* rxOut = _config.hasInput() ? &rx : nullptr;
	if (i2s_new_channel(&channelConfig, txOut, rxOut) != ESP_OK
	    || (_config.hasOutput() && tx == nullptr)
	    || (_config.hasInput() && rx == nullptr)) {
		return false;
	}

	i2s_std_config_t stdConfig{
	    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate),
	    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
	    .gpio_cfg = {
	                 .mclk = i2sGpio(_config.mclk),
	                 .bclk = i2sGpio(_config.bclk),
	                 .ws = i2sGpio(_config.wordSelect),
	                 .dout = i2sGpio(_config.dataOut),
	                 .din = i2sGpio(_config.dataIn),
	                 .invert_flags = {
	            .mclk_inv = false,
	            .bclk_inv = false,
	            .ws_inv = false,
	        },
	                 },
	};
	stdConfig.clk_cfg.clk_src = i2sClockSource();
	stdConfig.clk_cfg.mclk_multiple = es8311MclkMultiple;
	stdConfig.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
	stdConfig.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
	stdConfig.slot_cfg.ws_width = 16;

	if (tx != nullptr && i2s_channel_init_std_mode(tx, &stdConfig) != ESP_OK) {
		(void) i2s_del_channel(tx);
		if (rx != nullptr) {
			(void) i2s_del_channel(rx);
		}
		return false;
	}
	if (rx != nullptr && i2s_channel_init_std_mode(rx, &stdConfig) != ESP_OK) {
		if (tx != nullptr) {
			(void) i2s_del_channel(tx);
		}
		(void) i2s_del_channel(rx);
		return false;
	}
	if (tx != nullptr && i2s_channel_enable(tx) != ESP_OK) {
		if (rx != nullptr) {
			(void) i2s_del_channel(rx);
		}
		(void) i2s_del_channel(tx);
		return false;
	}
	if (rx != nullptr && i2s_channel_enable(rx) != ESP_OK) {
		if (tx != nullptr) {
			(void) i2s_channel_disable(tx);
			(void) i2s_del_channel(tx);
		}
		(void) i2s_del_channel(rx);
		return false;
	}

	_txHandle = tx;
	_rxHandle = rx;
	_currentSampleRateHz = sampleRate;
	_txEnabled = tx != nullptr;
	_rxEnabled = rx != nullptr;
	return true;
}

bool EspIdfEs8311AudioDevice::_initializeCodec() {
	for (auto const& reg : es8311OutputEnableSequence) {
		if (!_writeCodecRegister(reg.reg, reg.value)) {
			return false;
		}
		if (reg.reg == 0x00) {
			_sleepMs(5);
		}
	}
	_codecMuted = true;
	return true;
}

bool EspIdfEs8311AudioDevice::_writeCodecRegister(std::uint8_t reg, std::uint8_t value) {
	if (_controlBus == nullptr) {
		return false;
	}
	std::uint8_t const payload[] = {reg, value};
	return _controlBus->write(_config.i2cAddress, payload, sizeof(payload));
}

std::uint32_t EspIdfEs8311AudioDevice::_nativeSampleRateHz() const {
	return _config.sampleRateHz == 0 ? defaultNativeSampleRateHz : _config.sampleRateHz;
}

bool EspIdfEs8311AudioDevice::_ensureI2sSampleRate(std::uint32_t sampleRateHz) {
	if ((_txHandle == nullptr && _rxHandle == nullptr) || sampleRateHz == 0) {
		return false;
	}
	if (_currentSampleRateHz == sampleRateHz) {
		return true;
	}

	auto* const tx = static_cast<i2s_chan_handle_t>(_txHandle);
	auto* const rx = static_cast<i2s_chan_handle_t>(_rxHandle);
	if (_txEnabled && i2s_channel_disable(tx) != ESP_OK) {
		return false;
	}
	_txEnabled = false;
	if (_rxEnabled && i2s_channel_disable(rx) != ESP_OK) {
		return false;
	}
	_rxEnabled = false;

	i2s_std_clk_config_t clockConfig = I2S_STD_CLK_DEFAULT_CONFIG(sampleRateHz);
	clockConfig.clk_src = i2sClockSource();
	clockConfig.mclk_multiple = es8311MclkMultiple;
	if (tx != nullptr && i2s_channel_reconfig_std_clock(tx, &clockConfig) != ESP_OK) {
		return false;
	}
	if (rx != nullptr && i2s_channel_reconfig_std_clock(rx, &clockConfig) != ESP_OK) {
		return false;
	}

	if (tx != nullptr && i2s_channel_enable(tx) != ESP_OK) {
		return false;
	}
	_txEnabled = tx != nullptr;
	if (rx != nullptr && i2s_channel_enable(rx) != ESP_OK) {
		return false;
	}
	_rxEnabled = rx != nullptr;

	_currentSampleRateHz = sampleRateHz;
	return true;
}

bool EspIdfEs8311AudioDevice::_setCodecMute(bool mute) {
	auto const value = mute ? es8311DacMute : es8311DacUnmute;
	if (!_writeCodecRegister(0x31, value)) {
		return false;
	}
	_codecMuted = mute;
	return true;
}

bool EspIdfEs8311AudioDevice::_prepareOutput(std::uint32_t sampleRateHz) {
	if (!_ensureI2sSampleRate(sampleRateHz)) {
		return false;
	}
	if (!_codecMuted) {
		return true;
	}
	auto const primeFrames = static_cast<std::uint32_t>((static_cast<std::uint64_t>(sampleRateHz) * outputPrimeMs) / 1000ULL);
	if (primeFrames != 0 && !_writeSilence(primeFrames)) {
		return false;
	}
	if (!_setAmplifierEnabled(true)) {
		return false;
	}
	_sleepMs(5);
	return _setCodecMute(false);
}

bool EspIdfEs8311AudioDevice::_prepareInput() {
	if (_inputPrepared) {
		return true;
	}
	for (auto const& reg : es8311InputEnableSequence) {
		if (!_writeCodecRegister(reg.reg, reg.value)) {
			return false;
		}
	}
	_inputPrepared = true;
	return true;
}

void EspIdfEs8311AudioDevice::_resetPcmResampler() {
	_pcmInputSampleRateHz = 0;
	_resamplePhaseQ32 = 0;
	_resampleStepQ32 = 0;
	_resamplePrevious = {};
	_resampleOutputCount = 0;
	_resampleOutputChannel = AudioChannel::Stereo;
	_resampleHasPrevious = false;
}

bool EspIdfEs8311AudioDevice::_flushResampledOutput() {
	if (_resampleOutputCount == 0) {
		return true;
	}
	auto const ok = _writePcmFrames(_resampleOutput.data(), _resampleOutputCount, _resampleOutputChannel, false);
	_resampleOutputCount = 0;
	return ok;
}

bool EspIdfEs8311AudioDevice::_writePcmFramesResampled(
    AudioPcmFrame const* frames,
    std::size_t frameCount,
    AudioChannel channel,
    std::uint32_t inputSampleRateHz) {
	if (frames == nullptr || frameCount == 0 || inputSampleRateHz == 0) {
		return false;
	}

	auto const outputSampleRateHz = _nativeSampleRateHz();
	if (_pcmInputSampleRateHz != inputSampleRateHz || _resampleStepQ32 == 0) {
		_resetPcmResampler();
		_pcmInputSampleRateHz = inputSampleRateHz;
		_resampleStepQ32 = (static_cast<std::uint64_t>(inputSampleRateHz) << 32U) / outputSampleRateHz;
		if (_resampleStepQ32 == 0) {
			_resampleStepQ32 = 1;
		}
	}

	if (_resampleOutputCount != 0 && _resampleOutputChannel != channel && !_flushResampledOutput()) {
		return false;
	}
	_resampleOutputChannel = channel;

	if (!_resampleHasPrevious) {
		_resamplePrevious = frames[0];
		_resampleHasPrevious = true;
	}

	for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
		auto const current = frames[frameIndex];
		while (_resamplePhaseQ32 < resampleOneQ32) {
			_resampleOutput[_resampleOutputCount++] = interpolateFrame(_resamplePrevious, current, _resamplePhaseQ32);
			if (_resampleOutputCount >= _resampleOutput.size() && !_flushResampledOutput()) {
				return false;
			}
			_resamplePhaseQ32 += _resampleStepQ32;
		}
		_resamplePhaseQ32 -= resampleOneQ32;
		_resamplePrevious = current;
	}

	return true;
}

bool EspIdfEs8311AudioDevice::_writePcmFrames(
    AudioPcmFrame const* frames,
    std::size_t frameCount,
    AudioChannel channel,
    bool countStats) {
	if (_txHandle == nullptr || frames == nullptr) {
		return false;
	}

	auto* const tx = static_cast<i2s_chan_handle_t>(_txHandle);
	std::array<AudioPcmFrame, framesPerChunk> mapped{};
	std::size_t offset = 0;
	while (offset < frameCount) {
		auto const chunkFrames = frameCount - offset < mapped.size() ? frameCount - offset : mapped.size();
		for (std::size_t index = 0; index < chunkFrames; ++index) {
			auto const& frame = frames[offset + index];
			switch (channel) {
				case AudioChannel::Left:
					mapped[index] = AudioPcmFrame{.left = frame.left, .right = 0};
					break;
				case AudioChannel::Right:
					mapped[index] = AudioPcmFrame{.left = 0, .right = frame.right};
					break;
				case AudioChannel::Stereo:
					mapped[index] = frame;
					break;
			}
		}
		auto const bytes = chunkFrames * sizeof(AudioPcmFrame);
		std::size_t bytesWritten = 0;
		if (i2s_channel_write(tx, mapped.data(), bytes, &bytesWritten, 1000) != ESP_OK || bytesWritten != bytes) {
			return false;
		}
		if (countStats) {
			_pcmStats.framesQueued += chunkFrames;
			_pcmStats.framesWritten += chunkFrames;
		}
		offset += chunkFrames;
	}
	return true;
}

bool EspIdfEs8311AudioDevice::_writeSilence(std::uint32_t frames) {
	if (_txHandle == nullptr) {
		return false;
	}
	std::array<AudioPcmFrame, framesPerChunk> silence{};
	while (frames != 0) {
		auto const chunkFrames = frames < silence.size() ? frames : static_cast<std::uint32_t>(silence.size());
		if (!_writePcmFrames(silence.data(), chunkFrames, AudioChannel::Stereo, false)) {
			return false;
		}
		frames -= chunkFrames;
	}
	return true;
}

void EspIdfEs8311AudioDevice::_releaseI2s() {
	if (_txHandle == nullptr && _rxHandle == nullptr) {
		(void) _setAmplifierEnabled(false);
		return;
	}
	(void) _setAmplifierEnabled(false);
	auto* const tx = static_cast<i2s_chan_handle_t>(_txHandle);
	auto* const rx = static_cast<i2s_chan_handle_t>(_rxHandle);
	if (_txEnabled) {
		(void) i2s_channel_disable(tx);
	}
	if (_rxEnabled) {
		(void) i2s_channel_disable(rx);
	}
	if (tx != nullptr) {
		(void) i2s_del_channel(tx);
	}
	if (rx != nullptr) {
		(void) i2s_del_channel(rx);
	}
	_txHandle = nullptr;
	_rxHandle = nullptr;
	_currentSampleRateHz = 0;
	_txEnabled = false;
	_rxEnabled = false;
	_inputPrepared = false;
	_ready = false;
}

void EspIdfEs8311AudioDevice::_sleepMs(std::uint32_t milliseconds) const {
	if (_platform != nullptr && _platform->clock.sleepMs != nullptr) {
		_platform->clock.sleepMs(milliseconds);
		return;
	}
	lexxos::platform::sleepMs(milliseconds);
}

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
