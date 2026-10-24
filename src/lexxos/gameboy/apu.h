#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio_device.h>

namespace lexxos::gameboy {

class Apu final {
public:
	static constexpr std::uint32_t cpuClockHz = 4194304;
	static constexpr std::uint32_t sampleRateHz = 22050;
	static constexpr std::uint16_t audioRegisterStart = 0xFF10;
	static constexpr std::uint16_t controlRegisterEnd = 0xFF26;
	static constexpr std::uint16_t waveRamStart = 0xFF30;
	static constexpr std::uint16_t waveRamEnd = 0xFF3F;

	Apu() = default;

	void reset();
	void setPcmOutputEnabled(bool enabled);
	[[nodiscard]] bool pcmOutputEnabled() const;

	[[nodiscard]] static bool isAudioRegister(std::uint16_t address);
	[[nodiscard]] std::uint8_t readRegister(std::uint16_t address) const;
	void writeRegister(std::uint16_t address, std::uint8_t value);

	void advanceCycles(std::uint32_t cycles);

	void clearPcmFrames();
	[[nodiscard]] std::size_t audioFrameCount() const;
	[[nodiscard]] std::uint32_t droppedAudioFrames() const;
	[[nodiscard]] std::size_t readAudioFrames(lexxos::devices::AudioPcmFrame* frames, std::size_t maxFrames);

private:
	static constexpr std::uint16_t nr10 = 0xFF10;
	static constexpr std::uint16_t nr11 = 0xFF11;
	static constexpr std::uint16_t nr12 = 0xFF12;
	static constexpr std::uint16_t nr13 = 0xFF13;
	static constexpr std::uint16_t nr14 = 0xFF14;
	static constexpr std::uint16_t nr21 = 0xFF16;
	static constexpr std::uint16_t nr22 = 0xFF17;
	static constexpr std::uint16_t nr23 = 0xFF18;
	static constexpr std::uint16_t nr24 = 0xFF19;
	static constexpr std::uint16_t nr30 = 0xFF1A;
	static constexpr std::uint16_t nr31 = 0xFF1B;
	static constexpr std::uint16_t nr32 = 0xFF1C;
	static constexpr std::uint16_t nr33 = 0xFF1D;
	static constexpr std::uint16_t nr34 = 0xFF1E;
	static constexpr std::uint16_t nr41 = 0xFF20;
	static constexpr std::uint16_t nr42 = 0xFF21;
	static constexpr std::uint16_t nr43 = 0xFF22;
	static constexpr std::uint16_t nr44 = 0xFF23;
	static constexpr std::uint16_t nr50 = 0xFF24;
	static constexpr std::uint16_t nr51 = 0xFF25;
	static constexpr std::uint16_t nr52 = 0xFF26;

	static constexpr std::size_t controlRegisterCount = controlRegisterEnd - audioRegisterStart + 1U;
	static constexpr std::size_t waveRamSize = waveRamEnd - waveRamStart + 1U;
	static constexpr std::size_t pcmRingCapacity = 1536;

public:
	// Channel state stays owned by APU; public visibility is only needed by
	// translation-unit helper functions in apu.cpp on small embedded builds.
	struct SquareChannel {
		std::uint16_t frequency{0};
		std::uint32_t phase{0};
		std::uint32_t phaseStep{0};
		std::uint32_t lengthSamples{0};
		std::uint32_t envelopeSamples{0};
		std::uint8_t duty{0};
		std::uint8_t initialVolume{0};
		std::uint8_t currentVolume{0};
		std::uint8_t envelopePeriod{0};
		bool envelopeIncrease{false};
		bool lengthEnabled{false};
		bool dacEnabled{false};
		bool enabled{false};
	};

	struct WaveChannel {
		std::uint16_t frequency{0};
		std::uint32_t phase{0};
		std::uint32_t phaseStep{0};
		std::uint32_t lengthSamples{0};
		std::uint8_t volumeCode{0};
		bool lengthEnabled{false};
		bool dacEnabled{false};
		bool enabled{false};
	};

	struct NoiseChannel {
		std::uint32_t phase{0};
		std::uint32_t phaseStep{0};
		std::uint32_t lengthSamples{0};
		std::uint32_t envelopeSamples{0};
		std::uint16_t lfsr{0x7FFF};
		std::uint8_t initialVolume{0};
		std::uint8_t currentVolume{0};
		std::uint8_t envelopePeriod{0};
		bool envelopeIncrease{false};
		bool lengthEnabled{false};
		bool widthMode7Bit{false};
		bool dacEnabled{false};
		bool enabled{false};
	};

private:
	[[nodiscard]] std::uint8_t rawRegister(std::uint16_t address) const;
	void setRawRegister(std::uint16_t address, std::uint8_t value);
	void powerOff();
	void updateSquareFromRegisters(SquareChannel& channel, std::uint16_t nrx1, std::uint16_t nrx2, std::uint16_t nrx3, std::uint16_t nrx4);
	void triggerSquare(SquareChannel& channel, std::uint16_t nrx1, std::uint16_t nrx2, std::uint16_t nrx3, std::uint16_t nrx4);
	void updateWaveFromRegisters();
	void triggerWave();
	void updateNoiseFromRegisters();
	void triggerNoise();
	void stepSampleCounters();
	[[nodiscard]] lexxos::devices::AudioPcmFrame renderSample();
	void pushFrame(lexxos::devices::AudioPcmFrame frame);

	std::array<std::uint8_t, controlRegisterCount> _registers{};
	std::array<std::uint8_t, waveRamSize> _waveRam{};
	std::array<lexxos::devices::AudioPcmFrame, pcmRingCapacity> _pcmRing{};
	SquareChannel _square1{};
	SquareChannel _square2{};
	WaveChannel _wave{};
	NoiseChannel _noise{};
	std::uint64_t _sampleCycleAccumulator{0};
	std::size_t _pcmReadIndex{0};
	std::size_t _pcmWriteIndex{0};
	std::size_t _pcmFill{0};
	std::uint32_t _droppedAudioFrames{0};
	bool _masterEnabled{true};
	bool _pcmOutputEnabled{false};
};

} // namespace lexxos::gameboy
