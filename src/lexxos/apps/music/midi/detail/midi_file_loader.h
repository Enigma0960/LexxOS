#pragma once

#include <lexxos/apps/music/midi/detail/midi_types.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::music::midi::detail {

[[nodiscard]] char const* midiLoadStatusMessage(MidiLoadStatus status);
[[nodiscard]] MidiLoadStatus loadMidiFile(lexxos::vfs::FileHandle& file, MidiFile& midiFile);
[[nodiscard]] bool primeTracks(lexxos::vfs::FileHandle& file, MidiFile& midiFile);
[[nodiscard]] bool readNextTrackEvent(
    lexxos::vfs::FileHandle& file,
    TrackCursor& track,
    MidiState const& state);

} // namespace lexxos::apps::music::midi::detail
