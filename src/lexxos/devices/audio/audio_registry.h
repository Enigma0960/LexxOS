#pragma once

#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/microphone_device.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::audio {

[[nodiscard]] AudioDevice* primaryAudioDevice(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] MicrophoneDevice* primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform);

} // namespace lexxos::devices::audio
