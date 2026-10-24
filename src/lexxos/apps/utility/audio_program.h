#pragma once

#include <commands/command_selection.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc audio
 * @source src/lexxos/apps/utility/audio_program.h
 * @description
 * Показывает primary `AudioDevice` и проигрывает простой тестовый тон через переносимый audio contract. На PicoCalc это
 * пользовательский путь поверх `picocalc-audio`, в отличие от низкоуровневой GPIO/PWM диагностики `beep`.
 *
 * @parameters
 * - Без параметров или `info` — показать устройство, готовность, поддержку tone/mix и диапазоны.
 * - `left|right|stereo` — проиграть тон в выбранный канал.
 * - `--frequency <hz>` / `-f <hz>` — необязательная частота тона; допустимый диапазон сообщает устройство.
 * - `--duration <ms>` / `-d <ms>` — необязательная длительность; допустимый максимум сообщает устройство.
 * - `--volume <0..255>` / `-v <0..255>` — необязательная громкость PWM duty; по умолчанию `128`.
 *
 * @examples
 * ```text
 * lexx:/> audio
 * lexx:/> audio left
 * lexx:/> audio stereo --frequency 1200 --duration 200 --volume 96
 * ```
 *
 * @author
 * Новые уведомления, MIDI playback и будущие интерактивные приложения должны идти через `AudioDevice`, чтобы не
 * привязываться к конкретным GPIO/PWM выводам платы.
 * @end
 */
class AudioProgram {
public:
	[[nodiscard]] static lexxos::program::ProgramDescriptor descriptor() {
		return {
		    .name = "audio",
		    .help = "show or test the primary audio device",
		    .description = "Uses DeviceManager and the portable AudioDevice API to show board audio capabilities or play a simple test tone.",
		    .usage = "audio [info|left|right|stereo] [--frequency <hz>] [--duration <ms>] [--volume <0..255>]",
		    .arguments = lexxos::program::ProgramArgumentList{arguments, argumentCount},
		    .handler = run,
		    .legacy = false,
		    .enabled = true,
		};
	}

private:
	static constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
         .name = "target",
         .valueName = "info|left|right|stereo",
         .help = "target action or channel",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "frequency",
         .shortName = 'f',
         .valueName = "hz",
         .help = "tone frequency in Hz",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "duration",
         .shortName = 'd',
         .valueName = "ms",
         .help = "tone duration in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "volume",
         .shortName = 'v',
         .valueName = "0..255",
         .help = "PWM duty volume, default 128",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	};
	static constexpr std::size_t argumentCount = sizeof(arguments) / sizeof(arguments[0]);
	static constexpr std::uint8_t defaultVolume = 128;

	[[nodiscard]] static bool parseOptionalUnsigned(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t& value) {
		auto const text = arguments.value(name);
		if (text.empty()) {
			return true;
		}
		return parseUnsigned32(text, value);
	}

	[[nodiscard]] static bool parseChannel(std::string_view text, lexxos::devices::AudioChannel& channel) {
		if (text == "left") {
			channel = lexxos::devices::AudioChannel::Left;
			return true;
		}
		if (text == "right") {
			channel = lexxos::devices::AudioChannel::Right;
			return true;
		}
		if (text == "stereo" || text == "both") {
			channel = lexxos::devices::AudioChannel::Stereo;
			return true;
		}
		return false;
	}

	static void writeChannelSupport(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& device,
	    lexxos::devices::AudioChannel channel) {
		if (!device.supportsChannel(channel)) {
			return;
		}
		context.write(" ");
		context.write(lexxos::devices::toString(channel));
	}

	static int writeInfo(lexxos::os::AppContext& context, lexxos::devices::AudioDevice& device) {
		context.write("audio: ");
		context.write(device.name());
		context.writeLine("");
		context.write("state: ");
		context.writeLine(device.isReady() ? "ready" : "not-ready");
		context.write("tone: ");
		context.writeLine(device.supportsTone() ? "yes" : "no");
		context.write("mix: ");
		if (device.supportsToneMix()) {
			context.write("yes voices=");
			writeUnsigned(context, device.maxToneMixVoices());
		} else {
			context.write("no");
		}
		context.writeLine("");
		context.write("pcm: ");
		context.writeLine(device.supportsPcmBuffer() ? "yes" : "no");
		context.write("channels:");
		writeChannelSupport(context, device, lexxos::devices::AudioChannel::Left);
		writeChannelSupport(context, device, lexxos::devices::AudioChannel::Right);
		writeChannelSupport(context, device, lexxos::devices::AudioChannel::Stereo);
		context.writeLine("");
		context.write("frequency-hz: min=");
		writeUnsigned(context, device.minToneFrequencyHz());
		context.write(" max=");
		writeUnsigned(context, device.maxToneFrequencyHz());
		context.write(" default=");
		writeUnsigned(context, device.defaultToneFrequencyHz());
		context.writeLine("");
		context.write("duration-ms: max=");
		writeUnsigned(context, device.maxToneDurationMs());
		context.write(" default=");
		writeUnsigned(context, device.defaultToneDurationMs());
		context.writeLine("");
		context.write("volume: 0..255 default=");
		writeUnsigned(context, defaultVolume);
		context.writeLine("");
		return 0;
	}

#if LEXXOS_COMMAND_TEST_OPERATIONS
	static int runTone(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& device,
	    lexxos::devices::AudioChannel channel,
	    lexxos::program::ParsedArguments const& arguments) {
		lexxos::devices::AudioToneRequest request{
		    .channel = channel,
		    .frequencyHz = device.defaultToneFrequencyHz(),
		    .durationMs = device.defaultToneDurationMs(),
		    .volume = defaultVolume,
		};

		std::uint32_t volume = request.volume;
		if (!parseOptionalUnsigned(arguments, "frequency", request.frequencyHz)
		    || request.frequencyHz < device.minToneFrequencyHz()
		    || request.frequencyHz > device.maxToneFrequencyHz()) {
			context.writeErrorLine("audio: invalid --frequency");
			return 2;
		}
		if (!parseOptionalUnsigned(arguments, "duration", request.durationMs)
		    || request.durationMs == 0
		    || request.durationMs > device.maxToneDurationMs()) {
			context.writeErrorLine("audio: invalid --duration");
			return 2;
		}
		if (!parseOptionalUnsigned(arguments, "volume", volume) || volume > 255) {
			context.writeErrorLine("audio: invalid --volume");
			return 2;
		}
		request.volume = static_cast<std::uint8_t>(volume);

		if (!device.supportsChannel(channel)) {
			context.writeError("audio: unsupported channel: ");
			context.writeErrorLine(lexxos::devices::toString(channel));
			return 1;
		}

		context.write("audio: tone channel=");
		context.write(lexxos::devices::toString(channel));
		context.write(" frequency=");
		writeUnsigned(context, request.frequencyHz);
		context.write(" duration=");
		writeUnsigned(context, request.durationMs);
		context.write(" volume=");
		writeUnsigned(context, request.volume);
		context.writeLine("");
		return device.playTone(request) ? 0 : 1;
	}

#endif

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments) {
		auto* device = context.devices().primaryAudioDevice(context.platform());
		if (device == nullptr) {
			context.writeLine("audio: no primary audio device");
			return 1;
		}

		auto const target = arguments.positionalOr("target", "info");
		if (target.empty() || target == "info") {
			return writeInfo(context, *device);
		}

#if LEXXOS_COMMAND_TEST_OPERATIONS
		lexxos::devices::AudioChannel channel{};
		if (!parseChannel(target, channel)) {
			context.writeErrorLine("audio: expected info, left, right or stereo");
			return 2;
		}
		return runTone(context, *device, channel, arguments);
#else
		context.writeErrorLine("audio: tone tests are disabled in this build");
		return 2;
#endif
	}
};

} // namespace lexxos::apps::utility_programs
