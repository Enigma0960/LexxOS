#include <lexxos/apps/music/midi/detail/midi_renderer.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/apps/music/midi/detail/midi_file_loader.h>

namespace lexxos::apps::music::midi::detail {
namespace {

	struct Patch {
		MidiRenderWaveform waveform{MidiRenderWaveform::Triangle};
		std::uint16_t attackMs{5};
		std::uint16_t decayMs{80};
		std::uint8_t sustainLevel{150};
		std::uint16_t releaseMs{120};
		std::uint8_t gain{128};
	};

	static constexpr std::uint16_t midiNoteFrequencyHz[128] = {
	    8,
	    9,
	    9,
	    10,
	    10,
	    11,
	    12,
	    12,
	    13,
	    14,
	    15,
	    15,
	    16,
	    17,
	    18,
	    19,
	    21,
	    22,
	    23,
	    25,
	    26,
	    28,
	    29,
	    31,
	    33,
	    35,
	    37,
	    39,
	    41,
	    44,
	    46,
	    49,
	    52,
	    55,
	    58,
	    62,
	    65,
	    69,
	    73,
	    78,
	    82,
	    87,
	    92,
	    98,
	    104,
	    110,
	    117,
	    123,
	    131,
	    139,
	    147,
	    156,
	    165,
	    175,
	    185,
	    196,
	    208,
	    220,
	    233,
	    247,
	    262,
	    277,
	    294,
	    311,
	    330,
	    349,
	    370,
	    392,
	    415,
	    440,
	    466,
	    494,
	    523,
	    554,
	    587,
	    622,
	    659,
	    698,
	    740,
	    784,
	    831,
	    880,
	    932,
	    988,
	    1047,
	    1109,
	    1175,
	    1245,
	    1319,
	    1397,
	    1480,
	    1568,
	    1661,
	    1760,
	    1865,
	    1976,
	    2093,
	    2217,
	    2349,
	    2489,
	    2637,
	    2794,
	    2960,
	    3136,
	    3322,
	    3520,
	    3729,
	    3951,
	    4186,
	    4435,
	    4699,
	    4978,
	    5274,
	    5588,
	    5920,
	    6272,
	    6645,
	    7040,
	    7459,
	    7902,
	    8372,
	    8870,
	    9397,
	    9956,
	    10548,
	    11175,
	    11840,
	    12544,
	};

	static constexpr std::uint16_t masterMixFilterAlphaQ8 = 160;
	std::int32_t g_mixFilterLeftQ8 = 0;
	std::int32_t g_mixFilterRightQ8 = 0;

	void resetMixFilter() {
		g_mixFilterLeftQ8 = 0;
		g_mixFilterRightQ8 = 0;
	}

	std::int32_t filterMixSample(std::int32_t& stateQ8, std::int32_t sample) {
		auto const targetQ8 = static_cast<std::int64_t>(sample) << 8U;
		auto const deltaQ8 = targetQ8 - stateQ8;
		stateQ8 += static_cast<std::int32_t>((deltaQ8 * masterMixFilterAlphaQ8) / 256);
		return stateQ8 >> 8U;
	}

	void initializeRenderState(MidiRenderState& state) {
		state = MidiRenderState{};
		for (auto& channel : state.channels) {
			channel.program = 0;
			channel.volume = 100;
			channel.expression = 127;
			channel.pan = 64;
			channel.pitchBend = 8192;
			channel.sustain = false;
		}
		state.tempoUsPerQuarter = defaultTempoUsPerQuarter;
	}

	Patch patchFor(std::uint8_t program, std::uint8_t channel, std::uint8_t note) {
		if (channel == 9) {
			if (note == 35 || note == 36) {
				return Patch{MidiRenderWaveform::Sine, 1, 80, 0, 50, 240};
			}
			if (note == 38 || note == 40) {
				return Patch{MidiRenderWaveform::Noise, 1, 65, 0, 80, 180};
			}
			if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) {
				return Patch{MidiRenderWaveform::Triangle, 1, 90, 0, 70, 170};
			}
			if (note == 42 || note == 44 || note == 46) {
				return Patch{MidiRenderWaveform::Noise, 1, 22, 0, 24, 105};
			}
			if (note == 49 || note == 51 || note == 52 || note == 55 || note == 57 || note == 59) {
				return Patch{MidiRenderWaveform::Noise, 1, 120, 0, 160, 95};
			}
			return Patch{MidiRenderWaveform::Noise, 1, 45, 0, 55, 100};
		}

		if (program <= 7) {
			return Patch{MidiRenderWaveform::Triangle, 1, 650, 110, 380, 170};
		}
		if (program <= 15) {
			return Patch{MidiRenderWaveform::Sine, 1, 320, 20, 260, 145};
		}
		if (program <= 23) {
			return Patch{MidiRenderWaveform::Square, 8, 70, 225, 130, 115};
		}
		if (program <= 31) {
			return Patch{MidiRenderWaveform::Triangle, 2, 150, 42, 150, 145};
		}
		if (program <= 39) {
			return Patch{MidiRenderWaveform::Square, 3, 120, 135, 110, 165};
		}
		if (program <= 47) {
			return Patch{MidiRenderWaveform::Saw, 80, 240, 215, 420, 115};
		}
		if (program <= 55) {
			return Patch{MidiRenderWaveform::Sine, 90, 260, 220, 520, 105};
		}
		if (program <= 63) {
			return Patch{MidiRenderWaveform::Saw, 28, 160, 190, 220, 135};
		}
		if (program <= 71) {
			return Patch{MidiRenderWaveform::Square, 22, 130, 180, 180, 118};
		}
		if (program <= 79) {
			return Patch{MidiRenderWaveform::Sine, 24, 90, 220, 220, 120};
		}
		if (program <= 87) {
			return Patch{MidiRenderWaveform::Square, 4, 80, 180, 130, 130};
		}
		if (program <= 95) {
			return Patch{MidiRenderWaveform::Saw, 120, 320, 210, 650, 95};
		}
		if (program <= 103) {
			return Patch{MidiRenderWaveform::Noise, 30, 220, 90, 260, 80};
		}
		if (program <= 111) {
			return Patch{MidiRenderWaveform::Triangle, 2, 110, 35, 130, 135};
		}
		if (program <= 119) {
			return Patch{MidiRenderWaveform::Sine, 1, 180, 20, 140, 130};
		}
		return Patch{MidiRenderWaveform::Noise, 5, 180, 70, 180, 90};
	}

	std::uint32_t bendFrequency(std::uint16_t baseHz, std::uint16_t pitchBend) {
		auto bend = static_cast<std::int32_t>(pitchBend) - 8192;
		std::int32_t factorQ16 = 65536;
		if (bend >= 0) {
			factorQ16 += (bend * 8025) / 8191;
		} else {
			factorQ16 += (bend * 7135) / 8192;
		}
		if (factorQ16 < 1) {
			factorQ16 = 1;
		}
		return (static_cast<std::uint32_t>(baseHz) * static_cast<std::uint32_t>(factorQ16)) >> 16U;
	}

	std::uint32_t phaseStepFor(std::uint32_t frequencyHz, std::uint32_t sampleRateHz) {
		if (sampleRateHz == 0) {
			return 0;
		}
		return static_cast<std::uint32_t>((static_cast<std::uint64_t>(frequencyHz) << 32U) / sampleRateHz);
	}

	std::uint32_t initialPhaseFor(MidiEvent const& event) {
		auto seed = static_cast<std::uint32_t>(event.tick * 2654435761ULL);
		seed ^= static_cast<std::uint32_t>(event.a) << 24U;
		seed ^= static_cast<std::uint32_t>(event.channel) << 20U;
		seed ^= static_cast<std::uint32_t>(event.b) << 12U;
		seed ^= seed >> 16U;
		seed *= 2246822519U;
		seed ^= seed >> 13U;
		return seed;
	}

	std::uint32_t activeNoteCount(MidiRenderState const& state) {
		std::uint32_t count = 0;
		for (auto const& note : state.notes) {
			if (note.active) {
				++count;
			}
		}
		return count;
	}

	std::uint32_t voiceStealScore(MidiRenderedNote const& note) {
		if (!note.active) {
			return 0;
		}
		auto score = static_cast<std::uint32_t>(note.envLevel >> 8U) + static_cast<std::uint32_t>(note.velocity);
		if (note.envelope == MidiEnvelopeStage::Release || note.envelope == MidiEnvelopeStage::Off) {
			return score;
		}
		if (note.sustained) {
			return 512U + score;
		}
		if (note.envelope == MidiEnvelopeStage::Sustain) {
			return 1024U + score;
		}
		return 1536U + score;
	}

	MidiRenderedNote* findSameNoteSlot(MidiRenderState& state, MidiEvent const& event) {
		for (auto& note : state.notes) {
			if (note.active && note.channel == event.channel && note.note == event.a) {
				return &note;
			}
		}
		return nullptr;
	}

	MidiRenderedNote* freeNoteSlot(MidiRenderState& state) {
		for (auto& note : state.notes) {
			if (!note.active) {
				return &note;
			}
		}
		return nullptr;
	}

	MidiRenderedNote* stealNoteSlot(MidiRenderState& state, MidiEvent const& event) {
		if (auto* sameNote = findSameNoteSlot(state, event)) {
			return sameNote;
		}

		MidiRenderedNote* selected = nullptr;
		std::uint32_t selectedScore = 0xFFFFFFFFU;
		for (auto& note : state.notes) {
			auto const score = voiceStealScore(note);
			if (selected == nullptr || score < selectedScore) {
				selected = &note;
				selectedScore = score;
			}
		}
		return selected;
	}

	TrackCursor* nextTrack(MidiFile& midiFile) {
		TrackCursor* selected = nullptr;
		for (std::uint16_t index = 0; index < midiFile.state.trackCount; ++index) {
			auto& track = midiFile.tracks[index];
			if (!track.hasEvent) {
				continue;
			}
			if (selected == nullptr
			    || track.event.tick < selected->event.tick
			    || (track.event.tick == selected->event.tick && track.trackIndex < selected->trackIndex)) {
				selected = &track;
			}
		}
		return selected;
	}

	std::uint8_t clampMidi7(std::uint16_t value) {
		return value > 127U ? 127U : static_cast<std::uint8_t>(value);
	}

	std::uint16_t clampGainQ8(std::uint32_t value, std::uint16_t minGain, std::uint16_t maxGain) {
		if (minGain > maxGain) {
			auto const tmp = minGain;
			minGain = maxGain;
			maxGain = tmp;
		}
		if (value < minGain) {
			return minGain;
		}
		if (value > maxGain) {
			return maxGain;
		}
		return static_cast<std::uint16_t>(value);
	}

	std::uint16_t adaptiveBoostForGain(MidiRenderOptions const& options, std::uint16_t normalizationGainQ8) {
		if (!options.adaptiveBoost || normalizationGainQ8 <= 256U) {
			return 256;
		}
		auto const excess = static_cast<std::uint32_t>(normalizationGainQ8 - 256U);
		auto const boost = 256U + ((excess * options.adaptiveBoostSlopeQ8) / 256U);
		return clampGainQ8(boost, 256, options.adaptiveMaxBoostQ8);
	}

	void updateMaxActive(MidiRenderState const& state, MidiRenderStats& stats) {
		auto const active = activeNoteCount(state);
		if (active > stats.maxActiveNotes) {
			stats.maxActiveNotes = active;
		}
	}

	std::uint32_t abs32(std::int32_t value) {
		return value < 0 ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(value)) : static_cast<std::uint32_t>(value);
	}

	std::uint32_t integerSqrt(std::uint32_t value) {
		std::uint32_t result = 0;
		std::uint32_t bit = 1U << 30U;
		while (bit > value) {
			bit >>= 2U;
		}
		while (bit != 0) {
			if (value >= result + bit) {
				value -= result + bit;
				result = (result >> 1U) + bit;
			} else {
				result >>= 1U;
			}
			bit >>= 2U;
		}
		return result;
	}

	bool isNaturalDecayVoice(MidiRenderedNote const& voice) {
		return voice.channel != 9 && voice.program <= 7;
	}

	std::uint16_t envelopeStep(MidiRenderedNote& voice, std::uint32_t sampleRateHz) {
		static constexpr std::uint16_t maxEnvQ8 = 255U << 8U;
		auto samplesForMs = [sampleRateHz](std::uint16_t ms) -> std::uint32_t {
			auto samples = (static_cast<std::uint64_t>(sampleRateHz) * ms) / 1000ULL;
			return samples == 0 ? 1U : static_cast<std::uint32_t>(samples);
		};
		auto const sustainQ8 = static_cast<std::uint16_t>(static_cast<std::uint16_t>(voice.sustainLevel) << 8U);

		switch (voice.envelope) {
			case MidiEnvelopeStage::Off:
				voice.active = false;
				voice.envLevel = 0;
				break;
			case MidiEnvelopeStage::Attack: {
				auto const step = maxEnvQ8 / samplesForMs(voice.attackMs) + 1U;
				voice.envLevel = static_cast<std::uint16_t>(voice.envLevel + step);
				if (voice.envLevel >= maxEnvQ8 || voice.envLevel < step) {
					voice.envLevel = maxEnvQ8;
					voice.envelope = MidiEnvelopeStage::Decay;
				}
				break;
			}
			case MidiEnvelopeStage::Decay: {
				if (voice.envLevel <= sustainQ8) {
					voice.envLevel = sustainQ8;
					voice.envelope = MidiEnvelopeStage::Sustain;
					break;
				}
				auto const delta = static_cast<std::uint32_t>(voice.envLevel - sustainQ8);
				auto const step = delta / samplesForMs(voice.decayMs) + 1U;
				voice.envLevel = step >= voice.envLevel ? sustainQ8 : static_cast<std::uint16_t>(voice.envLevel - step);
				break;
			}
			case MidiEnvelopeStage::Sustain:
				if (isNaturalDecayVoice(voice)) {
					if (voice.envLevel == 0) {
						voice.envelope = MidiEnvelopeStage::Off;
						voice.active = false;
						break;
					}
					voice.envLevel = static_cast<std::uint16_t>(voice.envLevel - 1U);
				} else {
					voice.envLevel = sustainQ8;
				}
				break;
			case MidiEnvelopeStage::Release: {
				if (voice.envLevel == 0) {
					voice.envelope = MidiEnvelopeStage::Off;
					voice.active = false;
					break;
				}
				auto const step = static_cast<std::uint32_t>(voice.releaseStartLevel) / samplesForMs(voice.releaseMs) + 1U;
				voice.envLevel = step >= voice.envLevel ? 0 : static_cast<std::uint16_t>(voice.envLevel - step);
				if (voice.envLevel == 0) {
					voice.envelope = MidiEnvelopeStage::Off;
					voice.active = false;
				}
				break;
			}
		}
		return voice.envLevel > maxEnvQ8 ? 255U : static_cast<std::uint16_t>(voice.envLevel >> 8U);
	}

	std::int16_t sineSampleAtPhase(std::uint32_t phase) {
		static constexpr std::int16_t sineTable[32] = {
		    0,
		    6393,
		    12539,
		    18204,
		    23170,
		    27245,
		    30273,
		    32137,
		    32767,
		    32137,
		    30273,
		    27245,
		    23170,
		    18204,
		    12539,
		    6393,
		    0,
		    -6393,
		    -12539,
		    -18204,
		    -23170,
		    -27245,
		    -30273,
		    -32137,
		    -32767,
		    -32137,
		    -30273,
		    -27245,
		    -23170,
		    -18204,
		    -12539,
		    -6393,
		};
		auto const index = (phase >> 27U) & 0x1FU;
		auto const nextIndex = (index + 1U) & 0x1FU;
		auto const fraction = (phase >> 19U) & 0xFFU;
		auto const current = static_cast<std::int32_t>(sineTable[index]);
		auto const next = static_cast<std::int32_t>(sineTable[nextIndex]);
		auto const sample = current + (((next - current) * static_cast<std::int32_t>(fraction)) / 256);
		return static_cast<std::int16_t>(sample);
	}

	std::int16_t triangleSample(std::uint32_t phase);

	std::int16_t softSquareSample(std::uint32_t phase) {
		auto const sample = static_cast<std::int32_t>(triangleSample(phase)) * 3;
		if (sample > 32767) {
			return 32767;
		}
		if (sample < -32767) {
			return -32767;
		}
		return static_cast<std::int16_t>(sample);
	}

	std::int16_t softSawSample(std::uint32_t phase) {
		auto const position = static_cast<std::uint16_t>(phase >> 16U);
		auto const rawSaw = static_cast<std::int32_t>(position) - 32768;
		auto const softRamp = static_cast<std::int32_t>(triangleSample(phase + 0x40000000U));
		return static_cast<std::int16_t>(((softRamp * 7) + rawSaw) / 8);
	}

	std::int16_t triangleSample(std::uint32_t phase) {
		auto const position = static_cast<std::uint16_t>(phase >> 16U);
		auto const sample = position < 32768U
		    ? (static_cast<std::int32_t>(position) * 2) - 32767
		    : 98303 - (static_cast<std::int32_t>(position) * 2);
		return static_cast<std::int16_t>(sample);
	}

	std::int16_t waveformSampleAtPhase(MidiRenderWaveform waveform, std::uint32_t phase) {
		switch (waveform) {
			case MidiRenderWaveform::Sine:
				return sineSampleAtPhase(phase);
			case MidiRenderWaveform::Triangle:
				return triangleSample(phase);
			case MidiRenderWaveform::Square:
				return softSquareSample(phase);
			case MidiRenderWaveform::Saw:
				return softSawSample(phase);
			case MidiRenderWaveform::Noise:
				return 0;
		}
		return 0;
	}

	std::int16_t waveformSample(MidiRenderedNote& voice) {
		if (voice.waveform == MidiRenderWaveform::Noise) {
			voice.noise ^= voice.noise << 13U;
			voice.noise ^= voice.noise >> 17U;
			voice.noise ^= voice.noise << 5U;
			auto const target = static_cast<std::int32_t>((voice.noise >> 16U) & 0xFFFFU) - 32768;
			voice.noiseLevel += (target - voice.noiseLevel) / 4;
			return static_cast<std::int16_t>(voice.noiseLevel);
		}
		return waveformSampleAtPhase(voice.waveform, voice.phase);
	}

	std::uint16_t pianoHarmonicGainQ8(MidiRenderedNote const& voice, std::uint16_t env) {
		std::uint16_t gain = 16;
		switch (voice.envelope) {
			case MidiEnvelopeStage::Attack:
				gain = 56;
				break;
			case MidiEnvelopeStage::Decay:
				gain = 36;
				break;
			case MidiEnvelopeStage::Sustain:
				gain = 12;
				break;
			case MidiEnvelopeStage::Release:
				gain = 4;
				break;
			case MidiEnvelopeStage::Off:
				gain = 0;
				break;
		}
		if (env < 96U) {
			gain = static_cast<std::uint16_t>((static_cast<std::uint32_t>(gain) * env) / 96U);
		}
		return gain;
	}

	std::int32_t pianoVoiceSample(MidiRenderedNote const& voice, std::int32_t fundamental, std::uint16_t env) {
		if (voice.channel == 9 || voice.program > 7 || voice.waveform == MidiRenderWaveform::Noise) {
			return fundamental;
		}
		auto const phaseOffset = static_cast<std::uint32_t>((static_cast<std::uint32_t>(voice.note) * 0x1F123BB5U) ^ 0x6D2B79F5U);
		auto const octave = static_cast<std::int32_t>(waveformSampleAtPhase(voice.waveform, (voice.phase << 1U) + phaseOffset));
		auto const fifth = static_cast<std::int32_t>(waveformSampleAtPhase(voice.waveform, voice.phase + (phaseOffset >> 1U)));
		auto const harmonic = ((octave * 2) + fifth) / 3;
		auto const harmonicGain = pianoHarmonicGainQ8(voice, env);
		return static_cast<std::int32_t>(
		    ((static_cast<std::int64_t>(fundamental) * (256 - harmonicGain)) + (static_cast<std::int64_t>(harmonic) * harmonicGain))
		    / 256);
	}

	std::uint32_t voiceMixWeightQ8(MidiRenderedNote const& voice, std::uint16_t env) {
		if (env == 0) {
			return 0;
		}
		std::uint32_t weight = env;
		if (voice.envelope == MidiEnvelopeStage::Release || voice.sustained) {
			weight = (weight * 3U) / 4U;
		}
		if (isNaturalDecayVoice(voice) && weight < 24U) {
			weight = 0;
		}
		return weight;
	}

	std::uint16_t polyphonyMixGainQ8(std::uint32_t weightedVoicesQ8) {
		if (weightedVoicesQ8 <= 256U) {
			return 256;
		}
		auto const root = integerSqrt(weightedVoicesQ8);
		if (root == 0) {
			return 256;
		}
		auto gain = 4608U / root;
		if (gain > 256U) {
			gain = 256U;
		}
		return static_cast<std::uint16_t>(gain < 56U ? 56U : gain);
	}

	void panGains(std::uint8_t pan, std::int32_t& left, std::int32_t& right) {
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

	std::int16_t softLimit(std::int32_t value, MidiRenderStats& stats, bool after) {
		auto const inputPeak = abs32(value);
		if (!after && inputPeak > stats.peakBeforeLimiter) {
			stats.peakBeforeLimiter = inputPeak;
		}

		static constexpr std::int32_t limiterThreshold = 30000;
		static constexpr std::int32_t limiterCeiling = 32760;

		auto const sign = value < 0 ? -1 : 1;
		auto const magnitude = value < 0 ? -static_cast<std::int64_t>(value) : static_cast<std::int64_t>(value);
		std::int64_t limited = magnitude;
		if (magnitude > limiterThreshold) {
			auto const over = magnitude - limiterThreshold;
			limited = limiterThreshold + (over >> 2U);
		}
		if (limited > limiterCeiling) {
			limited = limiterCeiling;
		}

		auto const output = static_cast<std::int16_t>(sign * static_cast<std::int32_t>(limited));
		if (after) {
			auto const outputPeak = abs32(output);
			if (outputPeak > stats.peakAfterLimiter) {
				stats.peakAfterLimiter = outputPeak;
			}
		}
		return output;
	}

	lexxos::devices::AudioPcmFrame renderFrame(MidiRenderState& state, MidiRenderOptions const& options, MidiRenderStats& stats) {
		std::int32_t leftSum = 0;
		std::int32_t rightSum = 0;
		std::uint32_t weightedVoicesQ8 = 0;

		for (auto& voice : state.notes) {
			if (!voice.active) {
				continue;
			}
			voice.phase += voice.phaseStep;
			auto const env = envelopeStep(voice, options.sampleRateHz);
			if (!voice.active || env == 0) {
				continue;
			}
			weightedVoicesQ8 += voiceMixWeightQ8(voice, env);
			auto raw = static_cast<std::int32_t>(waveformSample(voice));
			raw = pianoVoiceSample(voice, raw, env);
			auto velocityGain = 48 + ((static_cast<std::int32_t>(voice.velocity) * 207) / 127);
			auto volumeGain = 32 + ((static_cast<std::int32_t>(voice.volume) * 223) / 127);
			auto sample = raw;
			sample = (sample * velocityGain) / 255;
			sample = (sample * volumeGain) / 255;
			sample = (sample * voice.expression) / 127;
			sample = (sample * env) / 255;
			sample = (sample * voice.gain) / 255;

			std::int32_t leftGain = 255;
			std::int32_t rightGain = 255;
			panGains(voice.pan, leftGain, rightGain);
			leftSum += (sample * leftGain) / 255;
			rightSum += (sample * rightGain) / 255;
		}

		auto const mixGainQ8 = polyphonyMixGainQ8(weightedVoicesQ8);
		leftSum = static_cast<std::int32_t>((static_cast<std::int64_t>(leftSum) * mixGainQ8) / 256);
		rightSum = static_cast<std::int32_t>((static_cast<std::int64_t>(rightSum) * mixGainQ8) / 256);
		leftSum = filterMixSample(g_mixFilterLeftQ8, leftSum);
		rightSum = filterMixSample(g_mixFilterRightQ8, rightSum);
		leftSum = (leftSum * options.masterVolume) / 255;
		rightSum = (rightSum * options.masterVolume) / 255;
		leftSum = static_cast<std::int32_t>((static_cast<std::int64_t>(leftSum) * options.outputGainQ8) / 256);
		rightSum = static_cast<std::int32_t>((static_cast<std::int64_t>(rightSum) * options.outputGainQ8) / 256);
		softLimit(leftSum, stats, false);
		softLimit(rightSum, stats, false);
		auto const left = softLimit(leftSum, stats, true);
		auto const right = softLimit(rightSum, stats, true);
		++stats.renderedFrames;
		return lexxos::devices::AudioPcmFrame{.left = left, .right = right};
	}

	bool outputFrames(
	    lexxos::devices::AudioDevice* audio,
	    std::array<lexxos::devices::AudioPcmFrame, maxRenderChunkFrames>& frames,
	    std::size_t frameCount,
	    MidiRenderOptions const& options) {
		if (frameCount == 0 || audio == nullptr) {
			return true;
		}
		return audio->playPcmBuffer(lexxos::devices::AudioPcmBufferRequest{
		    .channel = lexxos::devices::AudioChannel::Stereo,
		    .frames = frames.data(),
		    .frameCount = frameCount,
		    .sampleRateHz = options.sampleRateHz,
		});
	}

	bool renderFrames(
	    lexxos::devices::AudioDevice* audio,
	    MidiRenderState& state,
	    std::uint64_t framesToRender,
	    MidiRenderOptions const& options,
	    MidiRenderStats& stats) {
		static std::array<lexxos::devices::AudioPcmFrame, maxRenderChunkFrames> frames{};
		while (framesToRender > 0) {
			auto const chunkFrames = framesToRender > frames.size() ? frames.size() : static_cast<std::size_t>(framesToRender);
			for (std::size_t index = 0; index < chunkFrames; ++index) {
				frames[index] = renderFrame(state, options, stats);
			}
			if (!outputFrames(audio, frames, chunkFrames, options)) {
				return false;
			}
			framesToRender -= chunkFrames;
		}
		return true;
	}

	bool advanceTimeAndRender(
	    lexxos::devices::AudioDevice* audio,
	    MidiRenderState& state,
	    MidiEvent const& event,
	    MidiState const& midiState,
	    MidiRenderOptions const& options,
	    MidiRenderStats& stats) {
		if (event.tick < state.currentTick) {
			return true;
		}

		auto const deltaTicks = event.tick - state.currentTick;
		if (deltaTicks != 0) {
			++stats.segments;
			if (activeNoteCount(state) == 0) {
				++stats.silentSegments;
			}
		}

		std::uint64_t deltaUs = 0;
		if (midiState.division != 0) {
			deltaUs = (static_cast<std::uint64_t>(deltaTicks) * state.tempoUsPerQuarter) / midiState.division;
		}
		auto const framesToRender = (deltaUs * options.sampleRateHz) / 1000000ULL;
		if (!renderFrames(audio, state, framesToRender, options, stats)) {
			return false;
		}
		state.currentTimeUs += deltaUs;
		state.currentTick = event.tick;
		return true;
	}

	void releaseNote(MidiRenderedNote& note) {
		if (!note.active || note.envelope == MidiEnvelopeStage::Release || note.envelope == MidiEnvelopeStage::Off) {
			return;
		}
		note.sustained = false;
		note.releaseStartLevel = note.envLevel;
		note.envelope = MidiEnvelopeStage::Release;
	}

	void noteOn(MidiRenderState& state, MidiEvent const& event, MidiRenderOptions const& options, MidiRenderStats& stats) {
		if (event.channel >= midiChannelCount || event.a > 127U || event.b > 127U) {
			++stats.otherEvents;
			return;
		}

		auto* slot = freeNoteSlot(state);
		if (slot == nullptr) {
			slot = stealNoteSlot(state, event);
		}
		if (slot == nullptr) {
			++stats.droppedNotes;
			return;
		}

		auto const& channel = state.channels[event.channel];
		auto const patch = patchFor(channel.program, event.channel, static_cast<std::uint8_t>(event.a));
		auto const baseFrequency = midiNoteFrequencyHz[event.a];
		auto const frequencyHz = bendFrequency(baseFrequency, channel.pitchBend);
		*slot = MidiRenderedNote{
		    .channel = event.channel,
		    .note = static_cast<std::uint8_t>(event.a),
		    .velocity = static_cast<std::uint8_t>(event.b),
		    .program = channel.program,
		    .volume = channel.volume,
		    .expression = channel.expression,
		    .pan = channel.pan,
		    .pitchBend = channel.pitchBend,
		    .phase = initialPhaseFor(event),
		    .phaseStep = phaseStepFor(frequencyHz, options.sampleRateHz),
		    .noise = 0x12345678U ^ (static_cast<std::uint32_t>(event.a) << 16U) ^ static_cast<std::uint32_t>(event.channel),
		    .noiseLevel = 0,
		    .envLevel = 0,
		    .releaseStartLevel = 0,
		    .attackMs = patch.attackMs,
		    .decayMs = patch.decayMs,
		    .releaseMs = patch.releaseMs,
		    .sustainLevel = patch.sustainLevel,
		    .gain = patch.gain,
		    .waveform = patch.waveform,
		    .envelope = MidiEnvelopeStage::Attack,
		    .active = true,
		    .sustained = false,
		};
		++stats.noteOnEvents;
	}

	void noteOff(MidiRenderState& state, MidiEvent const& event, MidiRenderStats& stats) {
		if (event.channel >= midiChannelCount || event.a > 127U) {
			++stats.otherEvents;
			return;
		}

		auto const& channel = state.channels[event.channel];
		for (auto& note : state.notes) {
			if (!note.active || note.channel != event.channel || note.note != event.a) {
				continue;
			}
			if (channel.sustain) {
				note.sustained = true;
			} else {
				releaseNote(note);
			}
		}
		++stats.noteOffEvents;
	}

	void releaseSustainedNotes(MidiRenderState& state, std::uint8_t channel) {
		for (auto& note : state.notes) {
			if (note.active && note.channel == channel && note.sustained) {
				releaseNote(note);
			}
		}
	}

	void controlChange(MidiRenderState& state, MidiEvent const& event, MidiRenderStats& stats) {
		if (event.channel >= midiChannelCount) {
			++stats.otherEvents;
			return;
		}

		auto& channel = state.channels[event.channel];
		auto const value = clampMidi7(event.b);
		switch (event.a) {
			case 7:
				channel.volume = value;
				break;
			case 10:
				channel.pan = value;
				break;
			case 11:
				channel.expression = value;
				break;
			case 64: {
				auto const wasSustain = channel.sustain;
				channel.sustain = value >= 64U;
				if (wasSustain && !channel.sustain) {
					releaseSustainedNotes(state, event.channel);
				}
				break;
			}
			default:
				break;
		}
		++stats.controlEvents;
	}

	void programChange(MidiRenderState& state, MidiEvent const& event, MidiRenderStats& stats) {
		if (event.channel >= midiChannelCount) {
			++stats.otherEvents;
			return;
		}
		state.channels[event.channel].program = clampMidi7(event.a);
		++stats.programEvents;
	}

	void pitchBend(MidiRenderState& state, MidiEvent const& event, MidiRenderStats& stats) {
		if (event.channel >= midiChannelCount) {
			++stats.otherEvents;
			return;
		}
		state.channels[event.channel].pitchBend = event.a > 16383U ? 16383U : event.a;
		++stats.pitchBendEvents;
	}

	void dispatchEvent(MidiRenderState& state, MidiState& midiState, MidiEvent const& event, MidiRenderOptions const& options, MidiRenderStats& stats) {
		++stats.events;
		switch (event.type) {
			case MidiEventType::NoteOn:
				noteOn(state, event, options, stats);
				break;
			case MidiEventType::NoteOff:
				noteOff(state, event, stats);
				break;
			case MidiEventType::ControlChange:
				controlChange(state, event, stats);
				break;
			case MidiEventType::ProgramChange:
				programChange(state, event, stats);
				break;
			case MidiEventType::PitchBend:
				pitchBend(state, event, stats);
				break;
			case MidiEventType::SetTempo:
				if (event.tempo != 0) {
					state.tempoUsPerQuarter = event.tempo;
					midiState.tempo = event.tempo;
				}
				++stats.tempoEvents;
				break;
			case MidiEventType::EndOfTrack:
				break;
			case MidiEventType::Other:
				++stats.otherEvents;
				break;
		}
		updateMaxActive(state, stats);
	}

	bool renderMidiFilePass(
	    lexxos::os::AppContext& context,
	    lexxos::vfs::FileHandle& file,
	    MidiFile& midiFile,
	    lexxos::devices::AudioDevice* audio,
	    MidiRenderOptions const& options,
	    MidiRenderStats& stats) {
		stats = MidiRenderStats{};
		if (midiFile.state.division == 0 || options.sampleRateHz == 0) {
			return false;
		}
		if (!primeTracks(file, midiFile)) {
			return false;
		}

		static MidiRenderState state{};
		initializeRenderState(state);
		resetMixFilter();
		midiFile.state.tempo = state.tempoUsPerQuarter;

		while (options.maxEvents == 0 || stats.events < options.maxEvents) {
			if (context.pollCancellationRequest()) {
				break;
			}

			auto* track = nextTrack(midiFile);
			if (track == nullptr) {
				break;
			}

			auto const event = track->event;
			if (!advanceTimeAndRender(audio, state, event, midiFile.state, options, stats)) {
				return false;
			}
			dispatchEvent(state, midiFile.state, event, options, stats);
			stats.finalTick = state.currentTick;
			stats.renderedTimeUs = state.currentTimeUs;

			if (!readNextTrackEvent(file, *track, midiFile.state)) {
				return false;
			}
		}

		auto const tailFrames = (static_cast<std::uint64_t>(options.tailMs) * options.sampleRateHz) / 1000ULL;
		if (!renderFrames(audio, state, tailFrames, options, stats)) {
			return false;
		}
		stats.renderedTimeUs = state.currentTimeUs + (static_cast<std::uint64_t>(options.tailMs) * 1000ULL);
		return true;
	}

} // namespace

bool renderMidiFile(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    MidiFile& midiFile,
    lexxos::devices::AudioDevice& audio,
    MidiRenderOptions const& options,
    MidiRenderStats& stats) {
	stats = MidiRenderStats{};
	if (midiFile.state.division == 0 || options.sampleRateHz == 0 || !audio.supportsPcmBuffer()) {
		return false;
	}

	auto effectiveOptions = options;
	effectiveOptions.normalizeVolume = false;

	if (!renderMidiFilePass(context, file, midiFile, &audio, effectiveOptions, stats)) {
		return false;
	}
	stats.normalizationScanPeak = 0;
	stats.normalizationTargetPeak = 0;
	stats.normalizationGainQ8 = 256;
	stats.adaptiveBoostQ8 = 256;
	stats.effectiveOutputGainQ8 = effectiveOptions.outputGainQ8;
	return true;
}

} // namespace lexxos::apps::music::midi::detail
