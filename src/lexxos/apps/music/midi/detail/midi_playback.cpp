#include <lexxos/apps/music/midi/detail/midi_playback.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/music/midi/detail/midi_event_dump.h>
#include <lexxos/apps/music/midi/detail/midi_file_loader.h>
#include <lexxos/apps/music/midi/detail/midi_renderer.h>
#include <lexxos/apps/music/midi/detail/pcm_output_guard.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::music::midi::detail {

namespace {

	inline constexpr std::uint32_t pcmTestDefaultSampleRateHz = 44100;

	std::string_view toString(MidiFormat format) {
		switch (format) {
			case MidiFormat::SingleTrack: return "Single track [0]";
			case MidiFormat::MultiTrack : return "Multi track [1]";
			case MidiFormat::MultiSong  : return "Multi song [2]";
		}

		return "Unknown format";
	}

	void writeRenderStats(lexxos::os::AppContext& context, MidiRenderStats const& stats) {
		context.writeLine("MIDI render stats");
		context.write("Events: ");
		writeUnsigned(context, stats.events);
		context.writeLine("");
		context.write("Frames: ");
		writeUnsigned(context, stats.renderedFrames);
		context.writeLine("");
		context.write("Duration us: ");
		writeUnsigned64(context, stats.renderedTimeUs);
		context.writeLine("");
		context.write("Notes on/off: ");
		writeUnsigned(context, stats.noteOnEvents);
		context.write("/");
		writeUnsigned(context, stats.noteOffEvents);
		context.writeLine("");
		context.write("Max active notes: ");
		writeUnsigned(context, stats.maxActiveNotes);
		context.writeLine("");
		context.write("Dropped notes: ");
		writeUnsigned(context, stats.droppedNotes);
		context.writeLine("");
		context.write("Peak before/after limiter: ");
		writeUnsigned(context, stats.peakBeforeLimiter);
		context.write("/");
		writeUnsigned(context, stats.peakAfterLimiter);
		context.writeLine("");
	}

} // namespace

std::uint32_t commandPcmSampleRate(lexxos::devices::AudioDevice& audio) {
	auto const preferred = audio.preferredPcmSampleRateHz();
	return preferred != 0 ? preferred : pcmTestDefaultSampleRateHz;
}

int runMidiPlayback(lexxos::os::AppContext& context, char const* path, lexxos::devices::AudioDevice* audio, bool dump) {
	// Открытие файла и проверка MIDI-заголовка.
	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.writeError("mplayer: ");
		context.writeError(path);
		context.writeError(": ");
		context.writeErrorLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	MidiFile midiFile;
	auto const loadStatus = loadMidiFile(file, midiFile);
	if (loadStatus != MidiLoadStatus::Ok) {
		context.writeError("Failed to load midi file: ");
		context.writeError(path);
		context.writeError(": ");
		context.writeErrorLine(midiLoadStatusMessage(loadStatus));
		return static_cast<uint32_t>(loadStatus);
	}

	context.writeLine("Midi file info");
	context.write("Format: ");
	context.writeLine(toString(midiFile.state.format));
	context.write("Track count: ");
	writeUnsigned(context, midiFile.state.trackCount);
	context.writeLine("");
	context.write("Division: ");
	writeUnsigned(context, midiFile.state.division);
	context.writeLine("");

	// Диагностика событий использует тот же загруженный файл.
	if (dump) {
		return dumpMidiStream(context, file, midiFile) ? 0 : 1;
	}

	if (audio == nullptr || !audio->supportsPcmBuffer()) {
		context.writeErrorLine("mplay: PCM audio output is unavailable");
		return 1;
	}

	// Вывод PCM с остановкой устройства при любом выходе из функции.
	MidiRenderStats stats{};
	MidiRenderOptions options{};
	options.sampleRateHz = commandPcmSampleRate(*audio);
	PcmOutputGuard pcmOutputGuard{*audio};
	context.writeLine("Rendering MIDI to PCM output...");
	if (!renderMidiFile(context, file, midiFile, *audio, options, stats)) {
		context.writeErrorLine("mplay: MIDI render failed");
		return 1;
	}

	if (!audio->drainPcmOutput(3000)) {
		context.writeErrorLine("mplay: PCM output drain timed out; forced stop");
	}

	writeRenderStats(context, stats);
	return 0;
}

} // namespace lexxos::apps::music::midi::detail
