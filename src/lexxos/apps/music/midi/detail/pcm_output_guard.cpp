#include <lexxos/apps/music/midi/detail/pcm_output_guard.h>

#include <lexxos/devices/audio_device.h>

namespace lexxos::apps::music::midi::detail {

PcmOutputGuard::PcmOutputGuard(lexxos::devices::AudioDevice& audio)
    : _audio(&audio) {
}

PcmOutputGuard::~PcmOutputGuard() {
	if (_audio != nullptr) {
		_audio->stopPcmOutput();
	}
}

} // namespace lexxos::apps::music::midi::detail
