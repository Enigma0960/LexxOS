#include <lexxos/apps/utility/beep_program.h>

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor BeepProgram::descriptor() {
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

bool BeepProgram::parseOptionalUnsigned(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view name,
    std::uint32_t& value) {
	auto const text = arguments.value(name);
	if (text.empty()) {
		return true;
	}
	return parseUnsigned32(text, value);
}

int BeepProgram::run(
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

} // namespace lexxos::apps::utility_programs
