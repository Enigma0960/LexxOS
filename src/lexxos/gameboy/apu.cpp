#include <lexxos/gameboy/apu.h>

namespace lexxos::gameboy {
namespace {

	constexpr std::uint32_t lengthTickHz = 256;
	constexpr std::uint32_t envelopeTickHz = 64;
	constexpr std::uint32_t squareBaseHz = 131072;
	constexpr std::uint32_t waveBaseHz = 65536;

	[[nodiscard]] std::uint32_t samplesForTicks(std::uint32_t ticks, std::uint32_t tickHz) {
		auto const samples = (static_cast<std::uint64_t>(ticks) * Apu::sampleRateHz) / tickHz;
		return samples == 0 ? 1U : static_cast<std::uint32_t>(samples);
	}

	[[nodiscard]] std::uint32_t phaseStepForFrequency(std::uint32_t frequencyHz) {
		if (frequencyHz == 0) {
			return 0;
		}
		auto const step = (static_cast<std::uint64_t>(frequencyHz) << 32U) / Apu::sampleRateHz;
		return step > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<std::uint32_t>(step);
	}

	[[nodiscard]] std::uint32_t squareFrequencyHz(std::uint16_t gbFrequency) {
		if (gbFrequency >= 2048U) {
			return 0;
		}
		return squareBaseHz / static_cast<std::uint32_t>(2048U - gbFrequency);
	}

	[[nodiscard]] std::uint32_t waveFrequencyHz(std::uint16_t gbFrequency) {
		if (gbFrequency >= 2048U) {
			return 0;
		}
		return waveBaseHz / static_cast<std::uint32_t>(2048U - gbFrequency);
	}

	[[nodiscard]] std::uint16_t gbFrequency(std::uint8_t low, std::uint8_t high) {
		return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high & 0x07U) << 8U));
	}

	[[nodiscard]] std::uint32_t squareLengthSamples(std::uint8_t lengthRegister) {
		auto const length = static_cast<std::uint8_t>(lengthRegister & 0x3FU);
		return samplesForTicks(length == 0 ? 64U : 64U - length, lengthTickHz);
	}

	[[nodiscard]] std::uint32_t waveLengthSamples(std::uint8_t lengthRegister) {
		return samplesForTicks(lengthRegister == 0 ? 256U : 256U - lengthRegister, lengthTickHz);
	}

	[[nodiscard]] std::int16_t clampPcm(std::int32_t value) {
		if (value > 32767) {
			return 32767;
		}
		if (value < -32768) {
			return -32768;
		}
		return static_cast<std::int16_t>(value);
	}

	[[nodiscard]] std::int32_t squareSample(Apu::SquareChannel& channel) {
		if (!channel.enabled || !channel.dacEnabled || channel.currentVolume == 0 || channel.phaseStep == 0) {
			return 0;
		}
		channel.phase += channel.phaseStep;
		auto const step = static_cast<std::uint8_t>((channel.phase >> 29U) & 0x07U);
		static constexpr std::uint8_t highCounts[4] = {1, 2, 4, 6};
		auto const high = step < highCounts[channel.duty & 0x03U];
		auto const amplitude = static_cast<std::int32_t>(channel.currentVolume) * 520;
		return high ? amplitude : -amplitude;
	}

	[[nodiscard]] std::int32_t waveSample(Apu::WaveChannel& channel, std::array<std::uint8_t, 16> const& waveRam) {
		if (!channel.enabled || !channel.dacEnabled || channel.volumeCode == 0 || channel.phaseStep == 0) {
			return 0;
		}
		channel.phase += channel.phaseStep;
		auto const sampleIndex = static_cast<std::uint8_t>((channel.phase >> 27U) & 0x1FU);
		auto const packed = waveRam[sampleIndex >> 1U];
		auto const nibble = (sampleIndex & 0x01U) == 0 ? static_cast<std::uint8_t>(packed >> 4U) : static_cast<std::uint8_t>(packed & 0x0FU);
		auto sample = (static_cast<std::int32_t>(nibble) - 8) * 950;
		switch (channel.volumeCode & 0x03U) {
			case 1:
				return sample;
			case 2:
				return sample / 2;
			case 3:
				return sample / 4;
			default:
				return 0;
		}
	}

	void clockNoiseLfsr(Apu::NoiseChannel& channel) {
		auto const xored = static_cast<std::uint16_t>((channel.lfsr ^ (channel.lfsr >> 1U)) & 0x01U);
		channel.lfsr = static_cast<std::uint16_t>((channel.lfsr >> 1U) | (xored << 14U));
		if (channel.widthMode7Bit) {
			channel.lfsr = static_cast<std::uint16_t>((channel.lfsr & static_cast<std::uint16_t>(~0x40U)) | (xored << 6U));
		}
	}

	[[nodiscard]] std::int32_t noiseSample(Apu::NoiseChannel& channel) {
		if (!channel.enabled || !channel.dacEnabled || channel.currentVolume == 0 || channel.phaseStep == 0) {
			return 0;
		}
		auto const previous = channel.phase;
		channel.phase += channel.phaseStep;
		if (channel.phase < previous || channel.phaseStep >= 0x80000000U) {
			clockNoiseLfsr(channel);
		}
		auto const amplitude = static_cast<std::int32_t>(channel.currentVolume) * 500;
		return (channel.lfsr & 0x01U) == 0 ? amplitude : -amplitude;
	}

	void stepSquareLengthAndEnvelope(Apu::SquareChannel& channel) {
		if (channel.lengthEnabled && channel.lengthSamples != 0) {
			--channel.lengthSamples;
			if (channel.lengthSamples == 0) {
				channel.enabled = false;
			}
		}
		if (channel.enabled && channel.envelopePeriod != 0) {
			++channel.envelopeSamples;
			auto const periodSamples = samplesForTicks(channel.envelopePeriod, envelopeTickHz);
			if (channel.envelopeSamples >= periodSamples) {
				channel.envelopeSamples = 0;
				if (channel.envelopeIncrease) {
					if (channel.currentVolume < 15U) {
						++channel.currentVolume;
					}
				} else if (channel.currentVolume > 0) {
					--channel.currentVolume;
				}
			}
		}
	}

	void stepWaveLength(Apu::WaveChannel& channel) {
		if (channel.lengthEnabled && channel.lengthSamples != 0) {
			--channel.lengthSamples;
			if (channel.lengthSamples == 0) {
				channel.enabled = false;
			}
		}
	}

	void stepNoiseLengthAndEnvelope(Apu::NoiseChannel& channel) {
		if (channel.lengthEnabled && channel.lengthSamples != 0) {
			--channel.lengthSamples;
			if (channel.lengthSamples == 0) {
				channel.enabled = false;
			}
		}
		if (channel.enabled && channel.envelopePeriod != 0) {
			++channel.envelopeSamples;
			auto const periodSamples = samplesForTicks(channel.envelopePeriod, envelopeTickHz);
			if (channel.envelopeSamples >= periodSamples) {
				channel.envelopeSamples = 0;
				if (channel.envelopeIncrease) {
					if (channel.currentVolume < 15U) {
						++channel.currentVolume;
					}
				} else if (channel.currentVolume > 0) {
					--channel.currentVolume;
				}
			}
		}
	}

	[[nodiscard]] std::uint32_t noiseClockHz(std::uint8_t nr43) {
		auto const divisorCode = static_cast<std::uint8_t>(nr43 & 0x07U);
		auto const divisor = divisorCode == 0 ? 8U : static_cast<std::uint32_t>(divisorCode) * 16U;
		auto const shift = static_cast<std::uint8_t>((nr43 >> 4U) & 0x0FU);
		if (shift >= 14U) {
			return 1;
		}
		auto clock = 524288U / divisor;
		clock >>= shift;
		return clock == 0 ? 1U : clock;
	}

} // namespace

void Apu::reset() {
	_registers = {};
	_waveRam = {0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C, 0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA};
	_square1 = {};
	_square2 = {};
	_wave = {};
	_noise = {};
	_sampleCycleAccumulator = 0;
	_masterEnabled = true;

	setRawRegister(nr10, 0x80);
	setRawRegister(nr11, 0xBF);
	setRawRegister(nr12, 0xF3);
	setRawRegister(nr14, 0xBF);
	setRawRegister(nr21, 0x3F);
	setRawRegister(nr24, 0xBF);
	setRawRegister(nr30, 0x7F);
	setRawRegister(nr31, 0xFF);
	setRawRegister(nr32, 0x9F);
	setRawRegister(nr34, 0xBF);
	setRawRegister(nr41, 0xFF);
	setRawRegister(nr50, 0x77);
	setRawRegister(nr51, 0xF3);
	setRawRegister(nr52, 0xF1);
	clearPcmFrames();
}

void Apu::setPcmOutputEnabled(bool enabled) {
	_pcmOutputEnabled = enabled;
	if (!enabled) {
		clearPcmFrames();
	}
}

bool Apu::pcmOutputEnabled() const {
	return _pcmOutputEnabled;
}

bool Apu::isAudioRegister(std::uint16_t address) {
	return (address >= audioRegisterStart && address <= controlRegisterEnd)
	    || (address >= waveRamStart && address <= waveRamEnd);
}

std::uint8_t Apu::readRegister(std::uint16_t address) const {
	if (address >= waveRamStart && address <= waveRamEnd) {
		return _waveRam[static_cast<std::size_t>(address - waveRamStart)];
	}
	if (address < audioRegisterStart || address > controlRegisterEnd) {
		return 0xFF;
	}
	if (address == nr52) {
		auto value = static_cast<std::uint8_t>(_masterEnabled ? 0x80U : 0x00U);
		if (_square1.enabled) {
			value |= 0x01U;
		}
		if (_square2.enabled) {
			value |= 0x02U;
		}
		if (_wave.enabled) {
			value |= 0x04U;
		}
		if (_noise.enabled) {
			value |= 0x08U;
		}
		return static_cast<std::uint8_t>(0x70U | value);
	}

	auto const value = rawRegister(address);
	switch (address) {
		case nr10:
			return static_cast<std::uint8_t>(value | 0x80U);
		case nr11:
		case nr21:
			return static_cast<std::uint8_t>(value | 0x3FU);
		case nr13:
		case nr23:
		case nr33:
			return 0xFF;
		case nr14:
		case nr24:
		case nr34:
		case nr44:
			return static_cast<std::uint8_t>(value | 0xBFU);
		case nr30:
			return static_cast<std::uint8_t>(value | 0x7FU);
		case nr31:
			return 0xFF;
		case nr32:
			return static_cast<std::uint8_t>(value | 0x9FU);
		case nr41:
			return 0xFF;
		default:
			return value;
	}
}

void Apu::writeRegister(std::uint16_t address, std::uint8_t value) {
	if (address >= waveRamStart && address <= waveRamEnd) {
		_waveRam[static_cast<std::size_t>(address - waveRamStart)] = value;
		return;
	}
	if (address < audioRegisterStart || address > controlRegisterEnd) {
		return;
	}
	if (address == nr52) {
		if ((value & 0x80U) == 0) {
			powerOff();
		} else {
			_masterEnabled = true;
			setRawRegister(nr52, static_cast<std::uint8_t>(value | 0x80U));
		}
		return;
	}
	if (!_masterEnabled) {
		return;
	}

	setRawRegister(address, value);
	switch (address) {
		case nr11:
		case nr12:
		case nr13:
			updateSquareFromRegisters(_square1, nr11, nr12, nr13, nr14);
			break;
		case nr14:
			updateSquareFromRegisters(_square1, nr11, nr12, nr13, nr14);
			if ((value & 0x80U) != 0) {
				triggerSquare(_square1, nr11, nr12, nr13, nr14);
			}
			break;
		case nr21:
		case nr22:
		case nr23:
			updateSquareFromRegisters(_square2, nr21, nr22, nr23, nr24);
			break;
		case nr24:
			updateSquareFromRegisters(_square2, nr21, nr22, nr23, nr24);
			if ((value & 0x80U) != 0) {
				triggerSquare(_square2, nr21, nr22, nr23, nr24);
			}
			break;
		case nr30:
		case nr31:
		case nr32:
		case nr33:
			updateWaveFromRegisters();
			break;
		case nr34:
			updateWaveFromRegisters();
			if ((value & 0x80U) != 0) {
				triggerWave();
			}
			break;
		case nr41:
		case nr42:
		case nr43:
			updateNoiseFromRegisters();
			break;
		case nr44:
			updateNoiseFromRegisters();
			if ((value & 0x80U) != 0) {
				triggerNoise();
			}
			break;
		default:
			break;
	}
}

void Apu::advanceCycles(std::uint32_t cycles) {
	if (!_pcmOutputEnabled || !_masterEnabled || cycles == 0) {
		return;
	}
	_sampleCycleAccumulator += static_cast<std::uint64_t>(cycles) * sampleRateHz;
	while (_sampleCycleAccumulator >= cpuClockHz) {
		_sampleCycleAccumulator -= cpuClockHz;
		stepSampleCounters();
		pushFrame(renderSample());
	}
}

void Apu::clearPcmFrames() {
	_pcmReadIndex = 0;
	_pcmWriteIndex = 0;
	_pcmFill = 0;
	_droppedAudioFrames = 0;
}

std::size_t Apu::audioFrameCount() const {
	return _pcmFill;
}

std::uint32_t Apu::droppedAudioFrames() const {
	return _droppedAudioFrames;
}

std::size_t Apu::readAudioFrames(lexxos::devices::AudioPcmFrame* frames, std::size_t maxFrames) {
	if (frames == nullptr || maxFrames == 0) {
		return 0;
	}
	std::size_t count = 0;
	while (count < maxFrames && _pcmFill != 0) {
		frames[count++] = _pcmRing[_pcmReadIndex];
		_pcmReadIndex = (_pcmReadIndex + 1U) % pcmRingCapacity;
		--_pcmFill;
	}
	return count;
}

std::uint8_t Apu::rawRegister(std::uint16_t address) const {
	if (address < audioRegisterStart || address > controlRegisterEnd) {
		return 0xFF;
	}
	return _registers[static_cast<std::size_t>(address - audioRegisterStart)];
}

void Apu::setRawRegister(std::uint16_t address, std::uint8_t value) {
	if (address < audioRegisterStart || address > controlRegisterEnd) {
		return;
	}
	_registers[static_cast<std::size_t>(address - audioRegisterStart)] = value;
}

void Apu::powerOff() {
	_masterEnabled = false;
	_registers = {};
	_square1.enabled = false;
	_square2.enabled = false;
	_wave.enabled = false;
	_noise.enabled = false;
	_sampleCycleAccumulator = 0;
	clearPcmFrames();
}

void Apu::updateSquareFromRegisters(SquareChannel& channel, std::uint16_t nrx1, std::uint16_t nrx2, std::uint16_t nrx3, std::uint16_t nrx4) {
	auto const dutyLength = rawRegister(nrx1);
	auto const envelope = rawRegister(nrx2);
	auto const low = rawRegister(nrx3);
	auto const high = rawRegister(nrx4);
	channel.duty = static_cast<std::uint8_t>((dutyLength >> 6U) & 0x03U);
	channel.frequency = gbFrequency(low, high);
	channel.phaseStep = phaseStepForFrequency(squareFrequencyHz(channel.frequency));
	channel.lengthEnabled = (high & 0x40U) != 0;
	channel.dacEnabled = (envelope & 0xF8U) != 0;
	channel.initialVolume = static_cast<std::uint8_t>((envelope >> 4U) & 0x0FU);
	channel.envelopeIncrease = (envelope & 0x08U) != 0;
	channel.envelopePeriod = static_cast<std::uint8_t>(envelope & 0x07U);
	if (!channel.dacEnabled) {
		channel.enabled = false;
	}
}

void Apu::triggerSquare(SquareChannel& channel, std::uint16_t nrx1, std::uint16_t nrx2, std::uint16_t nrx3, std::uint16_t nrx4) {
	updateSquareFromRegisters(channel, nrx1, nrx2, nrx3, nrx4);
	channel.currentVolume = channel.initialVolume;
	channel.envelopeSamples = 0;
	channel.phase = 0;
	if (channel.lengthSamples == 0) {
		channel.lengthSamples = squareLengthSamples(rawRegister(nrx1));
	}
	channel.enabled = channel.dacEnabled;
}

void Apu::updateWaveFromRegisters() {
	auto const control = rawRegister(nr30);
	auto const low = rawRegister(nr33);
	auto const high = rawRegister(nr34);
	_wave.dacEnabled = (control & 0x80U) != 0;
	_wave.frequency = gbFrequency(low, high);
	_wave.phaseStep = phaseStepForFrequency(waveFrequencyHz(_wave.frequency));
	_wave.volumeCode = static_cast<std::uint8_t>((rawRegister(nr32) >> 5U) & 0x03U);
	_wave.lengthEnabled = (high & 0x40U) != 0;
	if (!_wave.dacEnabled) {
		_wave.enabled = false;
	}
}

void Apu::triggerWave() {
	updateWaveFromRegisters();
	_wave.phase = 0;
	if (_wave.lengthSamples == 0) {
		_wave.lengthSamples = waveLengthSamples(rawRegister(nr31));
	}
	_wave.enabled = _wave.dacEnabled;
}

void Apu::updateNoiseFromRegisters() {
	auto const envelope = rawRegister(nr42);
	auto const polynomial = rawRegister(nr43);
	auto const control = rawRegister(nr44);
	_noise.dacEnabled = (envelope & 0xF8U) != 0;
	_noise.initialVolume = static_cast<std::uint8_t>((envelope >> 4U) & 0x0FU);
	_noise.envelopeIncrease = (envelope & 0x08U) != 0;
	_noise.envelopePeriod = static_cast<std::uint8_t>(envelope & 0x07U);
	_noise.lengthEnabled = (control & 0x40U) != 0;
	_noise.widthMode7Bit = (polynomial & 0x08U) != 0;
	_noise.phaseStep = phaseStepForFrequency(noiseClockHz(polynomial));
	if (!_noise.dacEnabled) {
		_noise.enabled = false;
	}
}

void Apu::triggerNoise() {
	updateNoiseFromRegisters();
	_noise.currentVolume = _noise.initialVolume;
	_noise.envelopeSamples = 0;
	_noise.phase = 0;
	_noise.lfsr = 0x7FFF;
	if (_noise.lengthSamples == 0) {
		_noise.lengthSamples = squareLengthSamples(rawRegister(nr41));
	}
	_noise.enabled = _noise.dacEnabled;
}

void Apu::stepSampleCounters() {
	stepSquareLengthAndEnvelope(_square1);
	stepSquareLengthAndEnvelope(_square2);
	stepWaveLength(_wave);
	stepNoiseLengthAndEnvelope(_noise);
}

lexxos::devices::AudioPcmFrame Apu::renderSample() {
	if (!_masterEnabled) {
		return {};
	}

	std::int32_t channels[4] = {
	    squareSample(_square1),
	    squareSample(_square2),
	    waveSample(_wave, _waveRam),
	    noiseSample(_noise),
	};

	auto const routing = rawRegister(nr51);
	auto const volume = rawRegister(nr50);
	auto const rightVolume = static_cast<std::int32_t>((volume & 0x07U) + 1U);
	auto const leftVolume = static_cast<std::int32_t>(((volume >> 4U) & 0x07U) + 1U);
	std::int32_t left = 0;
	std::int32_t right = 0;
	for (std::uint8_t index = 0; index < 4U; ++index) {
		if ((routing & (1U << index)) != 0) {
			right += channels[index];
		}
		if ((routing & (1U << (index + 4U))) != 0) {
			left += channels[index];
		}
	}

	left = (left * leftVolume) / 8;
	right = (right * rightVolume) / 8;
	return lexxos::devices::AudioPcmFrame{.left = clampPcm(left), .right = clampPcm(right)};
}

void Apu::pushFrame(lexxos::devices::AudioPcmFrame frame) {
	if (_pcmFill >= pcmRingCapacity) {
		++_droppedAudioFrames;
		return;
	}
	_pcmRing[_pcmWriteIndex] = frame;
	_pcmWriteIndex = (_pcmWriteIndex + 1U) % pcmRingCapacity;
	++_pcmFill;
}

} // namespace lexxos::gameboy
