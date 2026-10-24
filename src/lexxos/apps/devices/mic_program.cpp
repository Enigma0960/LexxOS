#include <lexxos/apps/devices/mic_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/microphone_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	using lexxos::program::ProgramLaunchMode;

	constexpr std::size_t micChunkFrames = 512;
	constexpr std::uint32_t defaultLoopbackSeconds = 3;
	constexpr std::uint32_t defaultGain = 4;

	constexpr lexxos::program::ProgramArgumentSpec micArguments[] = {
	    {
         .name = "seconds",
         .shortName = 's',
         .valueName = "seconds",
         .help = "sample or loopback duration",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "3",
	     },
	    {
         .name = "gain",
         .shortName = 'g',
         .valueName = "1..16",
         .help = "digital gain for loopback output",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "4",
	     },
	    {
         .name = "action",
         .valueName = "action",
         .help = "info, sample or loopback",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "microphone device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	[[nodiscard]] std::int16_t applyGain(std::int16_t sample, std::uint32_t gain) {
		auto scaled = static_cast<std::int32_t>(sample) * static_cast<std::int32_t>(gain);
		if (scaled > std::numeric_limits<std::int16_t>::max()) {
			return std::numeric_limits<std::int16_t>::max();
		}
		if (scaled < std::numeric_limits<std::int16_t>::min()) {
			return std::numeric_limits<std::int16_t>::min();
		}
		return static_cast<std::int16_t>(scaled);
	}

	[[nodiscard]] std::uint32_t absSample(std::int16_t sample) {
		auto const value = static_cast<std::int32_t>(sample);
		return static_cast<std::uint32_t>(value < 0 ? -value : value);
	}

	struct MicStats {
		std::uint32_t peak{0};
		std::uint32_t average{0};
	};

	[[nodiscard]] MicStats computeStats(lexxos::devices::AudioPcmFrame const* frames, std::size_t frameCount) {
		std::uint64_t sum = 0;
		std::uint32_t peak = 0;
		for (std::size_t index = 0; index < frameCount; ++index) {
			auto const left = absSample(frames[index].left);
			auto const right = absSample(frames[index].right);
			auto const framePeak = left > right ? left : right;
			if (framePeak > peak) {
				peak = framePeak;
			}
			sum += left;
			sum += right;
		}
		auto const sampleCount = frameCount * 2U;
		return {
		    .peak = peak,
		    .average = sampleCount == 0 ? 0 : static_cast<std::uint32_t>(sum / sampleCount),
		};
	}

	void applyGainInPlace(lexxos::devices::AudioPcmFrame* frames, std::size_t frameCount, std::uint32_t gain) {
		if (gain <= 1) {
			return;
		}
		for (std::size_t index = 0; index < frameCount; ++index) {
			frames[index].left = applyGain(frames[index].left, gain);
			frames[index].right = applyGain(frames[index].right, gain);
		}
	}

	void writeInfo(lexxos::os::AppContext& context, lexxos::devices::MicrophoneDevice& mic) {
		context.write("mic: ");
		context.writeLine(mic.name());
		context.write("state: ");
		context.writeLine(mic.isReady() ? "ready" : "not-ready");
		context.write("pcm-capture: ");
		context.writeLine(mic.supportsPcmCapture() ? "yes" : "no");
		context.write("sample-rate-hz: ");
		writeUnsigned(context, mic.preferredPcmSampleRateHz());
		context.writeLine("");
	}

	[[nodiscard]] bool captureChunk(
	    lexxos::devices::MicrophoneDevice& mic,
	    std::array<lexxos::devices::AudioPcmFrame, micChunkFrames>& frames,
	    std::size_t& framesRead,
	    std::uint32_t sampleRateHz,
	    std::uint32_t timeoutMs = 1000) {
		return mic.capturePcm({
		    .frames = frames.data(),
		    .frameCapacity = frames.size(),
		    .framesRead = &framesRead,
		    .sampleRateHz = sampleRateHz,
		    .timeoutMs = timeoutMs,
		});
	}

	int runSample(lexxos::os::AppContext& context, lexxos::devices::MicrophoneDevice& mic) {
		std::array<lexxos::devices::AudioPcmFrame, micChunkFrames> frames{};
		std::size_t framesRead = 0;
		auto const sampleRateHz = mic.preferredPcmSampleRateHz();
		if (!captureChunk(mic, frames, framesRead, sampleRateHz)) {
			context.writeErrorLine("mic: capture failed");
			return 1;
		}
		auto const stats = computeStats(frames.data(), framesRead);
		context.write("mic: frames=");
		writeUnsigned(context, static_cast<std::uint32_t>(framesRead));
		context.write(" sample-rate=");
		writeUnsigned(context, sampleRateHz);
		context.write(" peak=");
		writeUnsigned(context, stats.peak);
		context.write(" average=");
		writeUnsigned(context, stats.average);
		context.writeLine("");
		return 0;
	}

	int runLoopback(
	    lexxos::os::AppContext& context,
	    lexxos::devices::MicrophoneDevice& mic,
	    lexxos::program::ParsedArguments const& arguments) {
		auto* audio = context.devices().primaryAudioDevice(context.platform());
		if (audio == nullptr || !audio->supportsPcmBuffer() || !audio->supportsChannel(lexxos::devices::AudioChannel::Stereo)) {
			context.writeErrorLine("mic: PCM audio output is unavailable");
			return 1;
		}

		std::uint32_t seconds = defaultLoopbackSeconds;
		std::uint32_t gain = defaultGain;
		if (!parseUnsigned32(arguments.valueOr("seconds", "3"), seconds) || seconds == 0 || seconds > 30) {
			context.writeErrorLine("mic: invalid seconds value");
			return 2;
		}
		if (!parseUnsigned32(arguments.valueOr("gain", "4"), gain) || gain == 0 || gain > 16) {
			context.writeErrorLine("mic: invalid gain value");
			return 2;
		}

		std::array<lexxos::devices::AudioPcmFrame, micChunkFrames> frames{};
		auto const sampleRateHz = mic.preferredPcmSampleRateHz();
		auto const startedMs = uptimeMs(context);
		auto const durationMs = seconds * 1000U;
		std::uint64_t totalFrames = 0;
		std::uint32_t chunks = 0;
		std::uint32_t peak = 0;

		context.write("mic: loopback seconds=");
		writeUnsigned(context, seconds);
		context.write(" gain=");
		writeUnsigned(context, gain);
		context.write(" sample-rate=");
		writeUnsigned(context, sampleRateHz);
		context.writeLine("");

		while (uptimeMs(context) - startedMs < durationMs) {
			if (context.pollCancellationRequest()) {
				audio->stopPcmOutput();
				context.writeLine("mic: canceled");
				return 130;
			}

			std::size_t framesRead = 0;
			if (!captureChunk(mic, frames, framesRead, sampleRateHz, 500)) {
				audio->stopPcmOutput();
				context.writeErrorLine("mic: capture failed");
				return 1;
			}
			auto const stats = computeStats(frames.data(), framesRead);
			if (stats.peak > peak) {
				peak = stats.peak;
			}
			applyGainInPlace(frames.data(), framesRead, gain);
			if (!audio->playPcmBuffer({
			        .channel = lexxos::devices::AudioChannel::Stereo,
			        .frames = frames.data(),
			        .frameCount = framesRead,
			        .sampleRateHz = sampleRateHz,
			    })) {
				audio->stopPcmOutput();
				context.writeErrorLine("mic: loopback output failed");
				return 1;
			}
			totalFrames += framesRead;
			++chunks;
		}

		(void) audio->drainPcmOutput(1000);
		context.write("mic: loopback frames=");
		writeUnsigned64(context, totalFrames);
		context.write(" chunks=");
		writeUnsigned(context, chunks);
		context.write(" peak=");
		writeUnsigned(context, peak);
		context.writeLine("");
		return 0;
	}

} // namespace

lexxos::program::ProgramDescriptor MicProgram::descriptor() {
	return {
	    .name = "mic",
	    .help = "show or test the primary microphone",
	    .description = "Shows the registered microphone, captures one PCM chunk, or loops microphone input back to the primary audio output.",
	    .usage = "mic [info|sample|loopback] [-s seconds] [-g 1..16] [name]",
	    .arguments = argumentList(micArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 10U * 1024U,
	                .priority = 1,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int MicProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positionalOr("action", "info");
	auto const requestedName = arguments.positional("name");
	auto* mic = !requestedName.empty() ? context.devices().microphoneDevice(context.platform(), requestedName)
	                                   : context.devices().primaryMicrophoneDevice(context.platform());
	if (mic == nullptr) {
		context.writeErrorLine("mic: microphone is not registered");
		return 1;
	}
	if (!mic->isReady()) {
		context.writeErrorLine("mic: microphone is not ready");
		return 1;
	}
	if (action == "info") {
		writeInfo(context, *mic);
		return 0;
	}
	if (action == "sample") {
		return runSample(context, *mic);
	}
	if (action == "loopback") {
		return runLoopback(context, *mic, arguments);
	}
	context.writeErrorLine("mic: supported actions are info, sample and loopback");
	return 2;
}

} // namespace lexxos::apps::device_programs
