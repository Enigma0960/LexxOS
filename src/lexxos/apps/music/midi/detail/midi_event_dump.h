#pragma once

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::vfs {

class FileHandle;

} // namespace lexxos::vfs

namespace lexxos::apps::music::midi::detail {

struct MidiFile;

[[nodiscard]] bool dumpMidiStream(lexxos::os::AppContext& context, lexxos::vfs::FileHandle& file, MidiFile& midiFile);

} // namespace lexxos::apps::music::midi::detail
