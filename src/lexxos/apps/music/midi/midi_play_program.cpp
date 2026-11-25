#include <lexxos/apps/music/midi/midi_play_program.h>

#include <array>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/music/midi/detail/midi_playback.h>
#include <lexxos/apps/music/midi/detail/midi_types.h>
#include <lexxos/apps/music/midi/detail/pcm_output_test.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::music::midi {

lexxos::program::ProgramDescriptor MidiPlayProgram::descriptor() {
	return {
	    .name = "mplay",
	    .help = "MIDI player",
	    .description = "",
	    .usage = "mplay [--test] | mplay [--dump] <midi>",
	    .arguments = lexxos::program::ProgramArgumentList{detail::mp_arguments, detail::mp_argumentCount},
	    .handler = run,
	    .runtime = {
	                                                      .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                                                      .stackSizeBytes = 8192,
	                                                      .priority = 1,
	                                                      .allowBackground = false,
	                                                      },
	    .legacy = false,
	    .enabled = true,
	};
}

int MidiPlayProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const& arguments) {
	auto* audio = context.devices().primaryAudioDevice(context.platform());
	if (arguments.has("test")) {
		if (audio == nullptr) {
			context.writeErrorLine("mplay: audio output is unavailable");
			return 1;
		}

		return detail::runPcmTest(context, *audio) ? 0 : 1;
	}

	auto const midiPath = arguments.positional("midi");
	if (midiPath.empty()) {
		context.writeErrorLine("mplay: missing MIDI file path");
		return 1;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, midiPath, path)) {
		return 1;
	}

	return detail::runMidiPlayback(context, path.data(), audio, arguments.has("dump"));
}

} // namespace lexxos::apps::music::midi
