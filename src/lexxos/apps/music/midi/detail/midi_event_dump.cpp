#include <lexxos/apps/music/midi/detail/midi_event_dump.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/music/midi/detail/midi_file_loader.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::music::midi::detail {

namespace {

	inline constexpr std::uint32_t maxDumpEvents = 300;

	std::string_view toString(MidiEventType type) {
		switch (type) {
			case MidiEventType::NoteOn       : return "note_on";
			case MidiEventType::NoteOff      : return "note_off";
			case MidiEventType::ProgramChange: return "program";
			case MidiEventType::ControlChange: return "control";
			case MidiEventType::PitchBend    : return "pitch_bend";
			case MidiEventType::SetTempo     : return "tempo";
			case MidiEventType::EndOfTrack   : return "end_track";
			case MidiEventType::Other        : return "other";
		}

		return "unknown";
	}

	TrackCursor* nextTrack(MidiFile& midiFile) {
		TrackCursor* selected = nullptr;
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

	void writeEventFields(lexxos::os::AppContext& context, MidiEvent const& event) {
		switch (event.type) {
			case MidiEventType::NoteOn:
			case MidiEventType::NoteOff:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " note=", event.a);
				writeNamedUnsigned(context, " vel=", event.b);
				break;
			case MidiEventType::ProgramChange:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " program=", event.a);
				break;
			case MidiEventType::ControlChange:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " cc=", event.a);
				writeNamedUnsigned(context, " value=", event.b);
				break;
			case MidiEventType::PitchBend:
				writeNamedUnsigned(context, " ch=", event.channel);
				writeNamedUnsigned(context, " value=", event.a);
				break;
			case MidiEventType::SetTempo:
				writeNamedUnsigned(context, " us_per_qn=", event.tempo);
				if (event.tempo != 0) {
					writeNamedUnsigned(context, " bpm=", 60000000U / event.tempo);
				}

				break;
			case MidiEventType::EndOfTrack:
				break;
			case MidiEventType::Other:
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
	    MidiEvent const& event) {
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

} // namespace

bool dumpMidiStream(lexxos::os::AppContext& context, vfs::FileHandle& file, MidiFile& midiFile) {
	if (midiFile.state.division == 0) {
		context.writeErrorLine("mplay: invalid MIDI division");
		return false;
	}

	if (!primeTracks(file, midiFile)) {
		context.writeErrorLine("mplay: failed to read first MIDI events");
		return false;
	}

	context.writeLine("MIDI event dump");
	context.write("Limit: ");
	writeUnsigned(context, maxDumpEvents);
	context.writeLine(" events");

	std::uint32_t count = 0;
	std::uint32_t currentTick = 0;
	std::uint32_t currentTempo = defaultTempoUsPerQuarter;
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

		if (event.type == MidiEventType::SetTempo && event.tempo != 0) {
			currentTempo = event.tempo;
			midiFile.state.tempo = currentTempo;
		}

		if (!readNextTrackEvent(file, *track, midiFile.state)) {
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

} // namespace lexxos::apps::music::midi::detail
