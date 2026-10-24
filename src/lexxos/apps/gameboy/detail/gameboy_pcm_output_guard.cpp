#include <lexxos/apps/gameboy/detail/gameboy_pcm_output_guard.h>

#include <lexxos/devices/audio_device.h>

namespace lexxos::apps::gameboy_programs::detail {

GameBoyPcmOutputGuard::GameBoyPcmOutputGuard(lexxos::devices::AudioDevice* audio)
    : _audio(audio) {
}

GameBoyPcmOutputGuard::~GameBoyPcmOutputGuard() {
	if (_audio != nullptr) {
		_audio->stopPcmOutput();
	}
}

} // namespace lexxos::apps::gameboy_programs::detail
