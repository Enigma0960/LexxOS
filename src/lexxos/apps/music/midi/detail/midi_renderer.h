#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/apps/music/midi/detail/midi_types.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::music::midi::detail {

inline constexpr std::uint8_t midiChannelCount = 16;
inline constexpr std::uint8_t maxRenderedNotes = 32;
inline constexpr std::size_t maxRenderChunkFrames = 128;
inline constexpr std::uint32_t midiNormalizationReferencePeak = 13000;

enum class MidiRenderWaveform : std::uint8_t {
	Sine,
	Triangle,
	Square,
	Saw,
	Noise,
};

enum class MidiEnvelopeStage : std::uint8_t {
	Off,
	Attack,
	Decay,
	Sustain,
	Release,
};

struct MidiRenderOptions {
	std::uint32_t sampleRateHz = 20000;
	std::uint8_t masterVolume = 220;
	std::uint32_t maxEvents = 0;
	std::uint32_t tailMs = 600;
	bool normalizeVolume = false;
	std::uint32_t normalizationTargetPeak = midiNormalizationReferencePeak;
	std::uint16_t normalizationMinGainQ8 = 64;
	std::uint16_t normalizationMaxGainQ8 = 8192;
	bool adaptiveBoost = true;
	std::uint16_t adaptiveBoostSlopeQ8 = 128;
	std::uint16_t adaptiveMaxBoostQ8 = 768;
	std::uint16_t outputGainQ8 = 352;
};

struct MidiRenderStats {
	std::uint32_t events = 0;
	std::uint32_t noteOnEvents = 0;
	std::uint32_t noteOffEvents = 0;
	std::uint32_t controlEvents = 0;
	std::uint32_t programEvents = 0;
	std::uint32_t tempoEvents = 0;
	std::uint32_t pitchBendEvents = 0;
	std::uint32_t otherEvents = 0;
	std::uint32_t segments = 0;
	std::uint32_t silentSegments = 0;
	std::uint32_t droppedNotes = 0;
	std::uint32_t maxActiveNotes = 0;
	std::uint32_t finalTick = 0;
	std::uint32_t renderedFrames = 0;
	std::uint32_t peakBeforeLimiter = 0;
	std::uint32_t peakAfterLimiter = 0;
	std::uint32_t normalizationScanPeak = 0;
	std::uint32_t normalizationTargetPeak = 0;
	std::uint16_t normalizationGainQ8 = 256;
	std::uint16_t adaptiveBoostQ8 = 256;
	std::uint16_t effectiveOutputGainQ8 = 256;
	std::uint64_t renderedTimeUs = 0;
};

struct MidiChannelRenderState {
	std::uint8_t program = 0;
	std::uint8_t volume = 100;
	std::uint8_t expression = 127;
	std::uint8_t pan = 64;
	std::uint16_t pitchBend = 8192;
	bool sustain = false;
};

struct MidiRenderedNote {
	std::uint8_t channel = 0;
	std::uint8_t note = 0;
	std::uint8_t velocity = 0;
	std::uint8_t program = 0;
	std::uint8_t volume = 100;
	std::uint8_t expression = 127;
	std::uint8_t pan = 64;
	std::uint16_t pitchBend = 8192;
	std::uint32_t phase = 0;
	std::uint32_t phaseStep = 0;
	std::uint32_t noise = 0x12345678U;
	std::int32_t noiseLevel = 0;
	std::uint16_t envLevel = 0;
	std::uint16_t releaseStartLevel = 0;
	std::uint16_t attackMs = 5;
	std::uint16_t decayMs = 80;
	std::uint16_t releaseMs = 120;
	std::uint8_t sustainLevel = 150;
	std::uint8_t gain = 128;
	MidiRenderWaveform waveform = MidiRenderWaveform::Triangle;
	MidiEnvelopeStage envelope = MidiEnvelopeStage::Off;
	bool active = false;
	bool sustained = false;
};

struct MidiRenderState {
	MidiChannelRenderState channels[midiChannelCount]{};
	MidiRenderedNote notes[maxRenderedNotes]{};
	std::uint32_t currentTick = 0;
	std::uint32_t tempoUsPerQuarter = defaultTempoUsPerQuarter;
	std::uint64_t currentTimeUs = 0;
};

[[nodiscard]] bool renderMidiFile(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    MidiFile& midiFile,
    lexxos::devices::AudioDevice& audio,
    MidiRenderOptions const& options,
    MidiRenderStats& stats);

} // namespace lexxos::apps::music::midi::detail
