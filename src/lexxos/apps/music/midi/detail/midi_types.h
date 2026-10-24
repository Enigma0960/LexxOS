#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <lexxos/program/arguments.h>

namespace lexxos::apps::music::midi::detail {

inline constexpr std::uint16_t maxMidiTracks = 16;
inline constexpr std::uint32_t defaultTempoUsPerQuarter = 500000;

inline constexpr lexxos::program::ProgramArgumentSpec mp_arguments[] = {
    {
     .name = "dump",
     .help = "Print MIDI parser diagnostics and event stream",
     .kind = lexxos::program::ProgramArgumentKind::Flag,
     .required = false,
     },
    {
     .name = "test",
     .help = "Play a deterministic PCM output test pattern",
     .kind = lexxos::program::ProgramArgumentKind::Flag,
     .required = false,
     },
    {
     .name = "midi",
     .valueName = "path",
     .help = "Standard MIDI file path",
     .kind = lexxos::program::ProgramArgumentKind::Positional,
     .valueType = lexxos::program::ProgramArgumentValueType::Path,
     .required = false,
     },
};
inline constexpr std::size_t mp_argumentCount = sizeof(mp_arguments) / sizeof(mp_arguments[0]);

enum class MidiLoadStatus : std::uint8_t {
	Ok,
	InvalidFile,
	InvalidHeader,
	UnsupportedTiming,
	TooManyTracks,
	CannotReadChunk,
	TruncatedChunk,
	MissingTracks,
};

enum class MidiFormat : std::uint8_t {
	SingleTrack = 0,
	MultiTrack = 1,
	MultiSong = 2,
};

enum class MidiEventType : std::uint8_t {
	NoteOn = 0,
	NoteOff = 1,
	ProgramChange = 2,
	ControlChange = 3,
	PitchBend = 4,
	SetTempo = 5,
	SetTemp = SetTempo,
	EndOfTrack = 6,
	Other = 7,
};

struct MidiEvent {
	std::uint32_t tick = 0;
	MidiEventType type = MidiEventType::Other;
	std::uint16_t trackIndex = 0;
	std::uint8_t channel = 0;
	std::uint8_t chanel = 0;
	std::uint16_t a = 0;
	std::uint16_t b = 0;
	std::uint32_t tempo = defaultTempoUsPerQuarter;
	std::uint32_t seconds = 0;
};

struct TrackCursor {
	std::uint64_t position = 0;
	std::uint64_t end = 0;
	std::uint32_t tick = 0;
	std::uint16_t trackIndex = 0;
	std::uint8_t runningStatus = 0;
	bool done = true;
	bool hasEvent = false;
	MidiEvent event{};
};

struct MidiState {
	MidiFormat format = MidiFormat::SingleTrack;
	std::uint16_t trackCount = 0;
	std::uint16_t division = 0;
	std::uint32_t tempo = defaultTempoUsPerQuarter;
	std::vector<MidiEvent> events{};
};

struct MidiFile {
	MidiState state{};
	std::array<TrackCursor, maxMidiTracks> tracks{};
};

} // namespace lexxos::apps::music::midi::detail
