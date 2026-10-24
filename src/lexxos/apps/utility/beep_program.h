#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc beep
 * @platforms sim espidf
 * @source src/lexxos/apps/utility/beep_program.h
 * @description
 * Проигрывает короткий диагностический тон через переносимый `AudioDevice`. В sim-сборках команда использует
 * `emu_pwm_audio` или `emu_adc_audio`, чтобы быстро проверить аудиовывод эмулятора без PicoCalc GPIO/PWM зависимостей.
 *
 * @parameters
 * - `--frequency <hz>` / `-f <hz>` — частота тона; по умолчанию берется из устройства.
 * - `--duration <ms>` / `-d <ms>` — длительность; по умолчанию берется из устройства.
 * - `--volume <0..255>` / `-v <0..255>` — громкость; по умолчанию `128`.
 *
 * @examples
 * ```text
 * lexx:/> beep
 * lexx:/> beep --frequency 1200 --duration 80 --volume 96
 * ```
 * @end
 */
class BeepProgram {
public:
	[[nodiscard]] static lexxos::program::ProgramDescriptor descriptor() {
		return {
		    .name = "beep",
		    .help = "play a short audio beep",
		    .description = "Plays a short diagnostic tone through the primary AudioDevice.",
		    .usage = "beep [--frequency <hz>] [--duration <ms>] [--volume <0..255>]",
		    .arguments = lexxos::program::ProgramArgumentList{arguments, argumentCount},
		    .handler = run,
		    .legacy = false,
		    .enabled = true,
		};
	}

private:
	static constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
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
         .help = "beep duration in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "volume",
         .shortName = 'v',
         .valueName = "0..255",
         .help = "tone volume, default 128",
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

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments) {
		auto* device = context.devices().primaryAudioDevice(context.platform());
		if (device == nullptr) {
			context.writeLine("beep: no primary audio device");
			return 1;
		}
		if (!device->supportsTone()) {
			context.writeLine("beep: tone output is unavailable");
			return 1;
		}

		lexxos::devices::AudioToneRequest request{
		    .channel = lexxos::devices::AudioChannel::Stereo,
		    .frequencyHz = device->defaultToneFrequencyHz(),
		    .durationMs = device->defaultToneDurationMs(),
		    .volume = defaultVolume,
		};

		std::uint32_t volume = request.volume;
		if (!parseOptionalUnsigned(arguments, "frequency", request.frequencyHz)
		    || request.frequencyHz < device->minToneFrequencyHz()
		    || request.frequencyHz > device->maxToneFrequencyHz()) {
			context.writeErrorLine("beep: invalid --frequency");
			return 2;
		}
		if (!parseOptionalUnsigned(arguments, "duration", request.durationMs)
		    || request.durationMs == 0
		    || request.durationMs > device->maxToneDurationMs()) {
			context.writeErrorLine("beep: invalid --duration");
			return 2;
		}
		if (!parseOptionalUnsigned(arguments, "volume", volume) || volume > 255) {
			context.writeErrorLine("beep: invalid --volume");
			return 2;
		}
		request.volume = static_cast<std::uint8_t>(volume);

		if (!device->supportsChannel(request.channel)) {
			context.writeLine("beep: stereo output is unavailable");
			return 1;
		}

		context.write("beep: tone frequency=");
		writeUnsigned(context, request.frequencyHz);
		context.write(" duration=");
		writeUnsigned(context, request.durationMs);
		context.write(" volume=");
		writeUnsigned(context, request.volume);
		context.writeLine("");
		return device->playTone(request) ? 0 : 1;
	}
};

} // namespace lexxos::apps::utility_programs
