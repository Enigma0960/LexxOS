#pragma once

#include <cstdint>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::devices {

class AudioDevice;

} // namespace lexxos::devices

namespace lexxos::apps::music::midi::detail {

[[nodiscard]] std::uint32_t commandPcmSampleRate(lexxos::devices::AudioDevice& audio);
[[nodiscard]] int runMidiPlayback(lexxos::os::AppContext& context, char const* path,
    lexxos::devices::AudioDevice* audio, bool dump);

} // namespace lexxos::apps::music::midi::detail
