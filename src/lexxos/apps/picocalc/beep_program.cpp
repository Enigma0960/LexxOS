#include <lexxos/apps/picocalc/beep_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/picocalc/detail/beep_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::picocalc_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec beepArguments[] = {
	    {
         .name = "frequency",
         .shortName = 'f',
         .valueName = "hz",
         .help = "PWM beep frequency in Hz",
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
	};

} // namespace

lexxos::program::ProgramDescriptor BeepProgram::descriptor() {
	return {
	    .name = "beep",
	    .help = "play a short board beep",
	    .description = "Plays a simple PWM beep through the audio output declared by the active board profile.",
	    .usage = "beep [--frequency <hz>] [--duration <ms>]",
	    .arguments = argumentList(beepArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int BeepProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const& board = activeBoard(context);
	if (!board.audioBeep.isValid()) {
		context.writeLine("beep: audio output is not configured");
		return 1;
	}

	std::uint32_t frequencyHz = board.audioBeep.defaultFrequencyHz == 0 ? 880 : board.audioBeep.defaultFrequencyHz;
	std::uint32_t durationMs = board.audioBeep.defaultDurationMs == 0 ? 120 : board.audioBeep.defaultDurationMs;
	if (!parseOptionalUnsignedArgument(arguments, "frequency", frequencyHz)
	    || frequencyHz < detail::minBeepFrequencyHz
	    || frequencyHz > detail::maxBeepFrequencyHz) {
		context.writeErrorLine("beep: invalid --frequency");
		return 2;
	}
	if (!parseOptionalUnsignedArgument(arguments, "duration", durationMs)
	    || durationMs == 0
	    || durationMs > detail::maxBeepDurationMs) {
		context.writeErrorLine("beep: invalid --duration");
		return 2;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	return detail::runBeepSequenceOnPico(context, board.audioBeep, frequencyHz, durationMs);
#else
	context.writeLine("beep: PWM output is unavailable on this platform");
	return 1;
#endif
}

} // namespace lexxos::apps::picocalc_programs
