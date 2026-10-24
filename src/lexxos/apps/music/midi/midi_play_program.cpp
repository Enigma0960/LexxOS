#include <lexxos/apps/music/midi/midi_play_program.h>

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/apps/music/midi/detail/midi_file_loader.h>
#include <lexxos/apps/music/midi/detail/midi_renderer.h>
#include <lexxos/apps/music/midi/detail/midi_types.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::music::midi {

namespace {
	inline constexpr std::uint32_t maxDumpEvents = 300;
	inline constexpr std::uint32_t pcmTestDefaultSampleRateHz = 44100;
	inline constexpr std::size_t pcmTestChunkFrames = 256;
	inline constexpr std::int32_t pcmTestLowAmplitude = 3000;
	inline constexpr std::int32_t pcmTestNormalAmplitude = 8000;
	inline constexpr std::uint32_t pcmTestFadeMs = 8;
	inline constexpr std::uint32_t pcmReferenceBeepFrequencyHz = 440;
	inline constexpr std::uint32_t pcmReferenceBeepDurationMs = 500;
	inline constexpr std::uint8_t pcmReferenceBeepVolume = 160;

	enum class PcmTestWaveform : std::uint8_t {
		Silence,
		Sine,
		Triangle,
	};

	class PcmOutputGuard final {
	public:
		explicit PcmOutputGuard(lexxos::devices::AudioDevice& audio)
		    : _audio(&audio) {
		}

		PcmOutputGuard(PcmOutputGuard const&) = delete;
		PcmOutputGuard& operator=(PcmOutputGuard const&) = delete;

		~PcmOutputGuard() {
			if (_audio != nullptr) {
				_audio->stopPcmOutput();
			}
		}

	private:
		lexxos::devices::AudioDevice* _audio;
	};

	std::string_view toString(detail::MidiFormat format) {
		switch (format) {
			case detail::MidiFormat::SingleTrack: return "Single track [0]";
			case detail::MidiFormat::MultiTrack : return "Multi track [1]";
			case detail::MidiFormat::MultiSong  : return "Multi song [2]";
		}
		return "Unknown format";
	}

	std::string_view toString(detail::MidiEventType type) {
		switch (type) {
			case detail::MidiEventType::NoteOn       : return "note_on";
			case detail::MidiEventType::NoteOff      : return "note_off";
			case detail::MidiEventType::ProgramChange: return "program";
			case detail::MidiEventType::ControlChange: return "control";
			case detail::MidiEventType::PitchBend    : return "pitch_bend";
			case detail::MidiEventType::SetTempo     : return "tempo";
			case detail::MidiEventType::EndOfTrack   : return "end_track";
			case detail::MidiEventType::Other        : return "other";
		}
		return "unknown";
	}

	detail::TrackCursor* nextTrack(detail::MidiFile& midiFile) {
		detail::TrackCursor* selected = nullptr;
		for (std::uint16_t index = 0; index < midiFile.state.trackCount; ++index) {
			auto& track = midiFile.tracks[index];
			if (!track.hasEvent) {
				continue;
			}
			if (selected == nullptr
			    || track.event.tick < selected->event.tick
			    || (track.event.tick == selected->event.tick && track.trackIndex < selected->trackIndex)) {
				selected = &track;
			}
		}
		return selected;
	}

	void writeNamedUnsigned(lexxos::os::AppContext& context, std::string_view name, std::uint32_t value) {
		context.write(name);
		writeUnsigned(context, value);
	}

	void writeEventFields(lexxos::os::AppContext& context, detail::MidiEvent const& event) {
		switch (event.type) {
			case detail::MidiEventType::NoteOn:
			case detail::MidiEventType::NoteOff:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " note=", event.a);
				writeNamedUnsigned(context, " vel=", event.b);
				break;
			case detail::MidiEventType::ProgramChange:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " program=", event.a);
				break;
			case detail::MidiEventType::ControlChange:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " cc=", event.a);
				writeNamedUnsigned(context, " value=", event.b);
				break;
			case detail::MidiEventType::PitchBend:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " value=", event.a);
				break;
			case detail::MidiEventType::SetTempo:
				writeNamedUnsigned(context, " us_per_qn=", event.tempo);
				if (event.tempo != 0) {
					writeNamedUnsigned(context, " bpm=", 60000000U / event.tempo);
				}
				break;
			case detail::MidiEventType::EndOfTrack:
				break;
			case detail::MidiEventType::Other:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " a=", event.a);
				writeNamedUnsigned(context, " b=", event.b);
				break;
		}
	}

	void writeDumpEvent(
	    lexxos::os::AppContext& context,
	    std::uint32_t index,
	    std::uint64_t timeUs,
	    detail::MidiEvent const& event) {
		writeUnsigned(context, index);
		writeNamedUnsigned(context, " tick=", event.tick);
		context.write(" time_us=");
		writeUnsigned64(context, timeUs);
		writeNamedUnsigned(context, " tr=", event.trackIndex);
		context.write(" ");
		context.write(toString(event.type));
		writeEventFields(context, event);
		context.writeLine("");
	}

	bool dumpMidiStream(lexxos::os::AppContext& context, vfs::FileHandle& file, detail::MidiFile& midiFile) {
		if (midiFile.state.division == 0) {
			context.writeErrorLine("mplay: invalid MIDI division");
			return false;
		}
		if (!detail::primeTracks(file, midiFile)) {
			context.writeErrorLine("mplay: failed to read first MIDI events");
			return false;
		}

		context.writeLine("MIDI event dump");
		context.write("Limit: ");
		writeUnsigned(context, maxDumpEvents);
		context.writeLine(" events");

		std::uint32_t count = 0;
		std::uint32_t currentTick = 0;
		std::uint32_t currentTempo = detail::defaultTempoUsPerQuarter;
		std::uint64_t currentTimeUs = 0;

		while (count < maxDumpEvents) {
			if (context.pollCancellationRequest()) {
				context.writeLine("dump cancelled");
				return true;
			}

			auto* track = nextTrack(midiFile);
			if (track == nullptr) {
				break;
			}

			auto const event = track->event;
			if (event.tick >= currentTick) {
				auto const deltaTicks = event.tick - currentTick;
				currentTimeUs += (static_cast<std::uint64_t>(deltaTicks) * currentTempo) / midiFile.state.division;
				currentTick = event.tick;
			}

			writeDumpEvent(context, count, currentTimeUs, event);
			++count;

			if (event.type == detail::MidiEventType::SetTempo && event.tempo != 0) {
				currentTempo = event.tempo;
				midiFile.state.tempo = currentTempo;
			}

			if (!detail::readNextTrackEvent(file, *track, midiFile.state)) {
				context.writeError("mplay: failed to read next MIDI event at track ");
				writeUnsigned(context, track->trackIndex);
				context.writeLine("");
				return false;
			}
		}

		context.write("Dumped events: ");
		writeUnsigned(context, count);
		context.writeLine("");
		if (nextTrack(midiFile) != nullptr) {
			context.writeLine("Dump truncated by event limit");
		}
		return true;
	}

	std::uint32_t phaseStepFor(std::uint32_t frequencyHz, std::uint32_t sampleRateHz) {
		return static_cast<std::uint32_t>((static_cast<std::uint64_t>(frequencyHz) << 32U) / sampleRateHz);
	}

	std::int16_t sineSample(std::uint32_t phase) {
		static constexpr std::int16_t sineTable[32] = {
		    0,
		    6393,
		    12539,
		    18204,
		    23170,
		    27245,
		    30273,
		    32137,
		    32767,
		    32137,
		    30273,
		    27245,
		    23170,
		    18204,
		    12539,
		    6393,
		    0,
		    -6393,
		    -12539,
		    -18204,
		    -23170,
		    -27245,
		    -30273,
		    -32137,
		    -32767,
		    -32137,
		    -30273,
		    -27245,
		    -23170,
		    -18204,
		    -12539,
		    -6393,
		};
		auto const index = (phase >> 27U) & 0x1FU;
		auto const nextIndex = (index + 1U) & 0x1FU;
		auto const fraction = (phase >> 19U) & 0xFFU;
		auto const current = static_cast<std::int32_t>(sineTable[index]);
		auto const next = static_cast<std::int32_t>(sineTable[nextIndex]);
		auto const sample = current + (((next - current) * static_cast<std::int32_t>(fraction)) / 256);
		return static_cast<std::int16_t>(sample);
	}

	std::int16_t triangleSample(std::uint32_t phase) {
		auto const top = static_cast<std::uint8_t>(phase >> 24U);
		auto const fullScale = top < 128U
		    ? (static_cast<std::int32_t>(top) * 512) - 32767
		    : 98301 - (static_cast<std::int32_t>(top) * 512);
		return static_cast<std::int16_t>(fullScale);
	}

	std::int16_t waveformSample(PcmTestWaveform waveform, std::uint32_t phase) {
		switch (waveform) {
			case PcmTestWaveform::Silence:
				return 0;
			case PcmTestWaveform::Sine:
				return sineSample(phase);
			case PcmTestWaveform::Triangle:
				return triangleSample(phase);
		}
		return 0;
	}

	std::uint32_t fadeGain(std::uint64_t frame, std::uint64_t totalFrames, std::uint32_t sampleRateHz) {
		auto const fadeFrames = (static_cast<std::uint64_t>(pcmTestFadeMs) * sampleRateHz) / 1000ULL;
		if (fadeFrames == 0 || totalFrames <= fadeFrames * 2ULL) {
			return 255;
		}
		if (frame < fadeFrames) {
			return static_cast<std::uint32_t>((frame * 255ULL) / fadeFrames);
		}
		auto const remaining = totalFrames - frame - 1ULL;
		if (remaining < fadeFrames) {
			return static_cast<std::uint32_t>((remaining * 255ULL) / fadeFrames);
		}
		return 255;
	}

	bool playPcmTestSection(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& audio,
	    PcmTestWaveform waveform,
	    std::uint32_t startFrequencyHz,
	    std::uint32_t endFrequencyHz,
	    std::uint32_t durationMs,
	    std::int32_t amplitude,
	    std::uint32_t sampleRateHz,
	    std::uint32_t& phase,
	    std::uint32_t& totalFrames) {
		std::array<lexxos::devices::AudioPcmFrame, pcmTestChunkFrames> frames{};
		auto framesRemaining = (static_cast<std::uint64_t>(durationMs) * sampleRateHz) / 1000ULL;
		auto const sectionFrames = framesRemaining == 0 ? 1ULL : framesRemaining;
		auto const startStep = phaseStepFor(startFrequencyHz, sampleRateHz);
		auto const endStep = phaseStepFor(endFrequencyHz, sampleRateHz);
		auto phaseStep = static_cast<std::int64_t>(startStep);
		auto const phaseStepDelta = sectionFrames > 1ULL
		    ? (static_cast<std::int64_t>(endStep) - static_cast<std::int64_t>(startStep)) / static_cast<std::int64_t>(sectionFrames - 1ULL)
		    : 0LL;
		std::uint64_t frameInSection = 0;
		while (framesRemaining > 0) {
			if (context.pollCancellationRequest()) {
				context.writeLine("PCM test cancelled");
				return false;
			}
			auto const chunkFrames = framesRemaining > frames.size() ? frames.size() : static_cast<std::size_t>(framesRemaining);
			for (std::size_t index = 0; index < chunkFrames; ++index) {
				if (waveform != PcmTestWaveform::Silence) {
					phase += static_cast<std::uint32_t>(phaseStep);
					phaseStep += phaseStepDelta;
				}
				auto sample = static_cast<std::int32_t>(waveformSample(waveform, phase));
				sample = (sample * amplitude) / 32767;
				sample = (sample * static_cast<std::int32_t>(fadeGain(frameInSection, sectionFrames, sampleRateHz))) / 255;
				frames[index] = lexxos::devices::AudioPcmFrame{.left = static_cast<std::int16_t>(sample), .right = static_cast<std::int16_t>(sample)};
				++frameInSection;
			}
			if (!audio.playPcmBuffer(lexxos::devices::AudioPcmBufferRequest{
			        .channel = lexxos::devices::AudioChannel::Stereo,
			        .frames = frames.data(),
			        .frameCount = chunkFrames,
			        .sampleRateHz = sampleRateHz,
			    })) {
				context.writeErrorLine("mplay: PCM test output failed");
				return false;
			}
			framesRemaining -= chunkFrames;
			totalFrames += static_cast<std::uint32_t>(chunkFrames);
		}
		return true;
	}

	bool playReferenceBeep(lexxos::os::AppContext& context, lexxos::devices::AudioDevice& audio) {
		if (!audio.supportsTone() || !audio.supportsChannel(lexxos::devices::AudioChannel::Stereo)) {
			context.writeLine("Reference beep skipped: tone output is unavailable");
			return true;
		}
		context.write("Reference beep: ");
		writeUnsigned(context, pcmReferenceBeepFrequencyHz);
		context.write(" Hz, ");
		writeUnsigned(context, pcmReferenceBeepDurationMs);
		context.writeLine(" ms");
		return audio.playTone(lexxos::devices::AudioToneRequest{
		    .channel = lexxos::devices::AudioChannel::Stereo,
		    .frequencyHz = pcmReferenceBeepFrequencyHz,
		    .durationMs = pcmReferenceBeepDurationMs,
		    .volume = pcmReferenceBeepVolume,
		});
	}

	void writePcmOutputStats(lexxos::os::AppContext& context, lexxos::devices::AudioPcmOutputStats const& stats) {
		context.writeLine("PCM output stats");
		context.write("Sample rate: ");
		writeUnsigned(context, stats.sampleRateHz);
		context.writeLine(" Hz");
		context.write("Buffer calls: ");
		writeUnsigned(context, stats.bufferCalls);
		context.writeLine("");
		context.write("Frames queued/written: ");
		writeUnsigned64(context, stats.framesQueued);
		context.write("/");
		writeUnsigned64(context, stats.framesWritten);
		context.writeLine("");
		context.write("Underruns: ");
		writeUnsigned(context, stats.underruns);
		context.writeLine("");
		context.write("Full waits: ");
		writeUnsigned(context, stats.fullWaits);
		context.writeLine("");
		context.write("Ring fill/capacity: ");
		writeUnsigned(context, stats.ringFill);
		context.write("/");
		writeUnsigned(context, stats.ringCapacity);
		context.writeLine("");
		context.write("Ring high watermark: ");
		writeUnsigned(context, stats.ringHighWatermark);
		context.writeLine("");
		context.write("Late frames: ");
		writeUnsigned(context, stats.lateFrames);
		context.writeLine("");
		context.write("Max lateness us: ");
		writeUnsigned(context, stats.maxLatenessUs);
		context.writeLine("");
		context.write("Resyncs: ");
		writeUnsigned(context, stats.resyncs);
		context.writeLine("");
	}

	std::uint32_t commandPcmSampleRate(lexxos::devices::AudioDevice& audio) {
		auto const preferred = audio.preferredPcmSampleRateHz();
		return preferred != 0 ? preferred : pcmTestDefaultSampleRateHz;
	}

	bool runPcmTest(lexxos::os::AppContext& context, lexxos::devices::AudioDevice& audio) {
		if (!audio.supportsPcmBuffer()) {
			context.writeErrorLine("mplay: PCM audio output is unavailable");
			return false;
		}
		auto const testSampleRateHz = commandPcmSampleRate(audio);
		context.writeLine("PCM output test");
		context.write("Sample rate: ");
		writeUnsigned(context, testSampleRateHz);
		context.writeLine(" Hz");
		context.write("Chunk frames: ");
		writeUnsigned(context, pcmTestChunkFrames);
		context.writeLine("");
		context.write("Low amplitude: ");
		writeUnsigned(context, pcmTestLowAmplitude);
		context.writeLine("");
		context.write("Normal amplitude: ");
		writeUnsigned(context, pcmTestNormalAmplitude);
		context.writeLine("");
		context.write("Fade: ");
		writeUnsigned(context, pcmTestFadeMs);
		context.writeLine(" ms");
		context.writeLine("Pattern: reference beep, silence, sine 440 low, sine 440 normal, sine 880 normal, triangle 440, sine sweep 220->1760, sine steps 220/440/880/1760");

		if (!playReferenceBeep(context, audio)) {
			context.writeErrorLine("mplay: reference beep failed");
			return false;
		}
		audio.resetPcmOutputStats();
		context.writeLine("PCM playback starts now; no console output between audio sections.");

		std::uint32_t phase = 0;
		std::uint32_t totalFrames = 0;
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Silence, 0, 0, 500, 0, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 440, 440, 1500, pcmTestLowAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 440, 440, 1500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 880, 880, 1500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Triangle, 440, 440, 1000, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 220, 1760, 1500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 220, 220, 500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 440, 440, 500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 880, 880, 500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Sine, 1760, 1760, 500, pcmTestNormalAmplitude, testSampleRateHz, phase, totalFrames)) {
			return false;
		}
		if (!playPcmTestSection(context, audio, PcmTestWaveform::Silence, 0, 0, 500, 0, testSampleRateHz, phase, totalFrames)) {
			return false;
		}

		if (!audio.drainPcmOutput(3000)) {
			context.writeLine("PCM output drain timed out");
		}
		context.write("PCM test frames: ");
		writeUnsigned(context, totalFrames);
		context.writeLine("");
		lexxos::devices::AudioPcmOutputStats outputStats{};
		if (audio.pcmOutputStats(outputStats)) {
			writePcmOutputStats(context, outputStats);
		}
		return true;
	}

	void writeRenderStats(lexxos::os::AppContext& context, detail::MidiRenderStats const& stats) {
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
		return runPcmTest(context, *audio) ? 0 : 1;
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

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.writeError("mplayer: ");
		context.writeError(path.data());
		context.writeError(": ");
		context.writeErrorLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	detail::MidiFile midiFile;
	auto const loadStatus = detail::loadMidiFile(file, midiFile);
	if (loadStatus != detail::MidiLoadStatus::Ok) {
		context.writeError("Failed to load midi file: ");
		context.writeError(path.data());
		context.writeError(": ");
		context.writeErrorLine(detail::midiLoadStatusMessage(loadStatus));
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

	if (arguments.has("dump")) {
		return dumpMidiStream(context, file, midiFile) ? 0 : 1;
	}

	if (audio == nullptr || !audio->supportsPcmBuffer()) {
		context.writeErrorLine("mplay: PCM audio output is unavailable");
		return 1;
	}

	detail::MidiRenderStats stats{};
	detail::MidiRenderOptions options{};
	options.sampleRateHz = commandPcmSampleRate(*audio);
	PcmOutputGuard pcmOutputGuard{*audio};
	context.writeLine("Rendering MIDI to PCM output...");
	if (!detail::renderMidiFile(context, file, midiFile, *audio, options, stats)) {
		context.writeErrorLine("mplay: MIDI render failed");
		return 1;
	}
	if (!audio->drainPcmOutput(3000)) {
		context.writeErrorLine("mplay: PCM output drain timed out; forced stop");
	}
	writeRenderStats(context, stats);
	return 0;
}

} // namespace lexxos::apps::music::midi
