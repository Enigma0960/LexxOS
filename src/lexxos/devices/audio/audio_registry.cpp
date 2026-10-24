#include <lexxos/devices/audio/audio_registry.h>

#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <lexxos/devices/audio/pico_pwm_pcm_audio_device.h>
#endif

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <lexxos/devices/audio/espidf_es8311_audio_device.h>
	#include <lexxos/devices/bus/i2c_bus.h>
#endif

#if defined(LEXXOS_PLATFORM_SIM)
	#include <sim_audio_device.h>
#endif

namespace lexxos::devices::audio {
namespace {

	[[nodiscard]] lexxos::platform::BoardDescriptor const& activeBoard(lexxos::platform::PlatformServices const& platform) {
		auto const& board = platform.board;
		return board.current != nullptr ? board.current() : lexxos::platform::board();
	}

#if defined(LEXXOS_PLATFORM_ESPIDF)
	EspIdfEs8311AudioDevice* es8311AudioDevice(lexxos::platform::PlatformServices const& platform) {
		auto const& board = activeBoard(platform);
		if (board.audioCodec.codec != lexxos::platform::AudioCodecKind::Es8311 || !board.audioCodec.isValid()) {
			return nullptr;
		}
		static EspIdfEs8311AudioDevice device{board.audioCodec, lexxos::devices::debugI2cBus(), platform};
		if (!device.isReady()) {
			(void) device.initialize();
		}
		return device.isReady() ? &device : nullptr;
	}
#endif

} // namespace

AudioDevice* primaryAudioDevice(lexxos::platform::PlatformServices const& platform) {
	auto const& board = activeBoard(platform);

#if defined(LEXXOS_PLATFORM_SIM)
	(void) board;
	auto& device = lexxos::emulator::simAudioDevice();
	return device.isReady() ? &device : nullptr;
#endif

	if (!board.audioBeep.isValid()) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
		return es8311AudioDevice(platform);
#endif
		return nullptr;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	static PicoPwmPcmAudioDevice device{board.audioBeep, platform};
	static bool initialized = false;
	if (!initialized) {
		(void) device.initialize();
		initialized = true;
	}
	return device.isReady() ? &device : nullptr;
#else
	(void) platform;
	return nullptr;
#endif
}

MicrophoneDevice* primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	auto* codec = es8311AudioDevice(platform);
	if (codec == nullptr || !codec->supportsPcmCapture()) {
		return nullptr;
	}
	static EspIdfEs8311MicrophoneDevice microphone{*codec};
	return microphone.isReady() ? &microphone : nullptr;
#else
	(void) platform;
	return nullptr;
#endif
}

} // namespace lexxos::devices::audio
