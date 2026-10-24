#include <lexxos/app/console_output.h>

#include <cstdint>

#include <lexxos/app/console_mirror.h>
#include <lexxos/devices/audio/audio_registry.h>
#include <lexxos/platform/platform_services.h>

namespace {

constexpr std::string_view clearScreenSequence = "\x1B[2J\x1B[H";
constexpr char bellCharacter = '\a';
constexpr std::uint32_t bellFrequencyHz = 1760;
constexpr std::uint32_t bellDurationMs = 35;
constexpr std::uint8_t bellVolume = 96;

struct InteractiveLineGuardState {
	lexxos::app::console_output::InteractiveLineGuard guard{};
	unsigned bypassDepth{0};
	bool externalLineOpen{false};
};

InteractiveLineGuardState g_interactiveLineGuard;

bool hasInteractiveLineGuard() {
	auto const& guard = g_interactiveLineGuard.guard;
	return guard.context != nullptr && guard.clear != nullptr && guard.redraw != nullptr;
}

bool shouldUseInteractiveLineGuard(std::string_view text) {
	return !text.empty() && hasInteractiveLineGuard() && g_interactiveLineGuard.bypassDepth == 0;
}

bool endsExternalLine(std::string_view text) {
	return !text.empty() && text.back() == '\n';
}

void runGuardCallback(void (*callback)(void* context)) {
	++g_interactiveLineGuard.bypassDepth;
	callback(g_interactiveLineGuard.guard.context);
	--g_interactiveLineGuard.bypassDepth;
}

void beforeExternalWrite(std::string_view text) {
	if (!shouldUseInteractiveLineGuard(text) || g_interactiveLineGuard.externalLineOpen) {
		return;
	}

	g_interactiveLineGuard.externalLineOpen = true;
	runGuardCallback(g_interactiveLineGuard.guard.clear);
}

void afterExternalWrite(std::string_view text) {
	if (!shouldUseInteractiveLineGuard(text) || !endsExternalLine(text)) {
		return;
	}

	g_interactiveLineGuard.externalLineOpen = false;
	runGuardCallback(g_interactiveLineGuard.guard.redraw);
}

void writePlatform(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	if (platform.console.write != nullptr) {
		platform.console.write(text);
	}
}

std::uint32_t clampBellFrequency(lexxos::devices::AudioDevice const& device) {
	auto frequency = bellFrequencyHz;
	if (frequency < device.minToneFrequencyHz()) {
		frequency = device.minToneFrequencyHz();
	}
	if (frequency > device.maxToneFrequencyHz()) {
		frequency = device.maxToneFrequencyHz();
	}
	return frequency;
}

std::uint32_t clampBellDuration(lexxos::devices::AudioDevice const& device) {
	auto duration = bellDurationMs;
	auto const maxDuration = device.maxToneDurationMs();
	if (maxDuration != 0 && duration > maxDuration) {
		duration = maxDuration;
	}
	return duration == 0 ? 1 : duration;
}

bool bellChannel(lexxos::devices::AudioDevice const& device, lexxos::devices::AudioChannel& channel) {
	if (device.supportsChannel(lexxos::devices::AudioChannel::Stereo)) {
		channel = lexxos::devices::AudioChannel::Stereo;
		return true;
	}
	if (device.supportsChannel(lexxos::devices::AudioChannel::Left)) {
		channel = lexxos::devices::AudioChannel::Left;
		return true;
	}
	if (device.supportsChannel(lexxos::devices::AudioChannel::Right)) {
		channel = lexxos::devices::AudioChannel::Right;
		return true;
	}
	return false;
}

void playBell(lexxos::platform::PlatformServices const& platform) {
	auto* device = lexxos::devices::audio::primaryAudioDevice(platform);
	if (device == nullptr || !device->isReady() || !device->supportsTone()) {
		return;
	}

	lexxos::devices::AudioChannel channel{};
	if (!bellChannel(*device, channel)) {
		return;
	}

	(void) device->playTone({
	    .channel = channel,
	    .frequencyHz = clampBellFrequency(*device),
	    .durationMs = clampBellDuration(*device),
	    .volume = bellVolume,
	});
}

void playBells(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	for (char const value : text) {
		if (value == bellCharacter) {
			playBell(platform);
		}
	}
}

} // namespace

namespace lexxos::app::console_output {

ScopedInteractiveLineGuardBypass::ScopedInteractiveLineGuardBypass()
    : _active(true) {
	++g_interactiveLineGuard.bypassDepth;
}

ScopedInteractiveLineGuardBypass::~ScopedInteractiveLineGuardBypass() {
	if (!_active || g_interactiveLineGuard.bypassDepth == 0) {
		return;
	}

	--g_interactiveLineGuard.bypassDepth;
}

void setInteractiveLineGuard(InteractiveLineGuard guard) {
	g_interactiveLineGuard.guard = guard;
	g_interactiveLineGuard.externalLineOpen = false;
}

void clearInteractiveLineGuard(void const* context) {
	if (g_interactiveLineGuard.guard.context != context) {
		return;
	}

	g_interactiveLineGuard.guard = {};
	g_interactiveLineGuard.externalLineOpen = false;
}

void write(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	beforeExternalWrite(text);
	writePlatform(platform, text);
	playBells(platform, text);
	lexxos::app::console_mirror::write(text);
	afterExternalWrite(text);
}

void writeLine(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	write(platform, text);
	write(platform, "\r\n");
}

bool writeNormalized(
    lexxos::platform::PlatformServices const& platform,
    std::string_view text,
    bool& lastWasCarriageReturn) {
	std::size_t segmentStart = 0;
	for (std::size_t index = 0; index < text.size(); ++index) {
		char const value = text[index];
		if (value != '\n') {
			lastWasCarriageReturn = value == '\r';
			continue;
		}

		if (index > segmentStart) {
			write(platform, text.substr(segmentStart, index - segmentStart));
		}

		if (!lastWasCarriageReturn) {
			write(platform, "\r");
		}
		write(platform, "\n");

		lastWasCarriageReturn = false;
		segmentStart = index + 1;
	}

	if (segmentStart < text.size()) {
		write(platform, text.substr(segmentStart));
	}

	return true;
}

void clear(lexxos::platform::PlatformServices const& platform) {
	writePlatform(platform, clearScreenSequence);
	lexxos::app::console_mirror::clear();
}

} // namespace lexxos::app::console_output
