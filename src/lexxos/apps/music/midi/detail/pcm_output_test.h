#pragma once

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::devices {

class AudioDevice;

} // namespace lexxos::devices

namespace lexxos::apps::music::midi::detail {

[[nodiscard]] bool runPcmTest(lexxos::os::AppContext& context, lexxos::devices::AudioDevice& audio);

} // namespace lexxos::apps::music::midi::detail
