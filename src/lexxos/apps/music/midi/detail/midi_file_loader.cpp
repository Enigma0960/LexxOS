#include <lexxos/apps/music/midi/detail/midi_file_loader.h>

#include <cstddef>
#include <cstdint>

#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::music::midi::detail {
namespace {

	bool readBytes(vfs::FileHandle& file, void* data, std::size_t size) {
		std::size_t bytesRead = 0;
		return file.read(data, size, bytesRead) == vfs::ReadStatus::Ok && bytesRead == size;
	}

	bool readBytes(
	    vfs::FileHandle& file,
	    std::uint64_t& position,
	    std::uint64_t end,
	    void* data,
	    std::size_t size) {
		if (position > end || size > end - position || !readBytes(file, data, size)) {
			return false;
		}
		position += size;
		return true;
	}

	bool readU8(vfs::FileHandle& file, std::uint64_t& position, std::uint64_t end, std::uint8_t& value) {
		return readBytes(file, position, end, &value, 1);
	}

	bool readU16(vfs::FileHandle& file, std::uint64_t& position, std::uint64_t end, std::uint16_t& value) {
		std::uint8_t bytes[2]{};
		if (!readBytes(file, position, end, bytes, sizeof(bytes))) {
			return false;
		}

		value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
		return true;
	}

	bool readU32(vfs::FileHandle& file, std::uint64_t& position, std::uint64_t end, std::uint32_t& value) {
		std::uint8_t bytes[4]{};
		if (!readBytes(file, position, end, bytes, sizeof(bytes))) {
			return false;
		}

		value = (static_cast<std::uint32_t>(bytes[0]) << 24U)
		    | (static_cast<std::uint32_t>(bytes[1]) << 16U)
		    | (static_cast<std::uint32_t>(bytes[2]) << 8U)
		    | bytes[3];
		return true;
	}

	bool readVariable(vfs::FileHandle& file, std::uint64_t& position, std::uint64_t end, std::uint32_t& value) {
		value = 0;
		for (std::uint8_t index = 0; index < 4; ++index) {
			std::uint8_t byte = 0;
			if (!readU8(file, position, end, byte)) {
				return false;
			}

			value = (value << 7U) | static_cast<std::uint32_t>(byte & 0x7FU);
			if ((byte & 0x80U) == 0) {
				return true;
			}
		}

		return false;
	}

	bool seekForward(vfs::FileHandle& file, std::uint64_t& position, std::uint64_t end, std::uint32_t size) {
		if (position > end || size > end - position) {
			return false;
		}
		position += size;
		return file.seek(position);
	}

	bool chunkIdEquals(char const* actual, char const* expected) {
		return actual[0] == expected[0]
		    && actual[1] == expected[1]
		    && actual[2] == expected[2]
		    && actual[3] == expected[3];
	}

	bool readDataByte(
	    vfs::FileHandle& file,
	    std::uint64_t& position,
	    std::uint64_t end,
	    bool& hasFirstData,
	    std::uint8_t& firstData,
	    std::uint8_t& value) {
		if (hasFirstData) {
			value = firstData;
			hasFirstData = false;
			return true;
		}
		return readU8(file, position, end, value);
	}

	MidiEvent makeBaseEvent(TrackCursor const& track, MidiState const& state) {
		return MidiEvent{
		    .tick = track.tick,
		    .type = MidiEventType::Other,
		    .trackIndex = track.trackIndex,
		    .channel = 0,
		    .chanel = 0,
		    .a = 0,
		    .b = 0,
		    .tempo = state.tempo,
		    .seconds = 0,
		};
	}

	void setEventChannel(MidiEvent& event, std::uint8_t channel) {
		event.channel = channel;
		event.chanel = channel;
	}

} // namespace

char const* midiLoadStatusMessage(MidiLoadStatus status) {
	switch (status) {
		case MidiLoadStatus::Ok:
			return "";
		case MidiLoadStatus::InvalidFile:
			return "invalid MIDI file";
		case MidiLoadStatus::InvalidHeader:
			return "invalid MIDI header";
		case MidiLoadStatus::UnsupportedTiming:
			return "unsupported MIDI timing";
		case MidiLoadStatus::TooManyTracks:
			return "too many MIDI tracks";
		case MidiLoadStatus::CannotReadChunk:
			return "cannot read MIDI chunk";
		case MidiLoadStatus::TruncatedChunk:
			return "truncated MIDI chunk";
		case MidiLoadStatus::MissingTracks:
			return "missing MIDI track chunks";
	}
	return "unknown MIDI error";
}

MidiLoadStatus loadMidiFile(vfs::FileHandle& file, MidiFile& midiFile) {
	midiFile = MidiFile{};

	std::uint64_t position = 0;
	auto const fileSize = file.size();
	char chunkId[4]{};
	std::uint32_t chunkSize = 0;

	if (!file.seek(0)
	    || !readBytes(file, position, fileSize, chunkId, 4)
	    || !readU32(file, position, fileSize, chunkSize)) {
		return MidiLoadStatus::InvalidFile;
	}
	if (!chunkIdEquals(chunkId, "MThd")) {
		return MidiLoadStatus::InvalidHeader;
	}
	if (chunkSize < 6 || chunkSize > fileSize - position) {
		return MidiLoadStatus::InvalidHeader;
	}

	auto const headerEnd = position + chunkSize;
	std::uint16_t formatType = 0;
	std::uint16_t expectedTracks = 0;
	std::uint16_t division = 0;
	if (!readU16(file, position, headerEnd, formatType)
	    || !readU16(file, position, headerEnd, expectedTracks)
	    || !readU16(file, position, headerEnd, division)) {
		return MidiLoadStatus::InvalidHeader;
	}
	if (!file.seek(headerEnd)) {
		return MidiLoadStatus::InvalidHeader;
	}
	position = headerEnd;

	if (formatType > static_cast<std::uint16_t>(MidiFormat::MultiTrack)
	    || (division & 0x8000U) != 0) {
		return MidiLoadStatus::UnsupportedTiming;
	}
	if (formatType == static_cast<std::uint16_t>(MidiFormat::SingleTrack) && expectedTracks != 1) {
		return MidiLoadStatus::InvalidHeader;
	}
	if (expectedTracks > maxMidiTracks) {
		return MidiLoadStatus::TooManyTracks;
	}

	midiFile.state.format = static_cast<MidiFormat>(formatType);
	midiFile.state.trackCount = expectedTracks;
	midiFile.state.division = division;
	midiFile.state.tempo = defaultTempoUsPerQuarter;
	midiFile.state.events.clear();

	std::uint16_t loadedTracks = 0;
	while (position + 8 <= fileSize && loadedTracks < expectedTracks) {
		if (!file.seek(position)
		    || !readBytes(file, position, fileSize, chunkId, 4)
		    || !readU32(file, position, fileSize, chunkSize)) {
			return MidiLoadStatus::CannotReadChunk;
		}

		auto const trackBegin = position;
		if (chunkSize > fileSize - trackBegin) {
			return MidiLoadStatus::TruncatedChunk;
		}
		auto const trackEnd = trackBegin + chunkSize;

		if (chunkIdEquals(chunkId, "MTrk")) {
			midiFile.tracks[loadedTracks] = TrackCursor{
			    .position = trackBegin,
			    .end = trackEnd,
			    .tick = 0,
			    .trackIndex = loadedTracks,
			    .runningStatus = 0,
			    .done = false,
			    .hasEvent = false,
			    .event = MidiEvent{},
			};
			++loadedTracks;
		}

		position = trackEnd;
	}

	if (loadedTracks != expectedTracks) {
		return MidiLoadStatus::MissingTracks;
	}

	return MidiLoadStatus::Ok;
}

bool primeTracks(vfs::FileHandle& file, MidiFile& midiFile) {
	for (std::uint16_t index = 0; index < midiFile.state.trackCount; ++index) {
		if (!readNextTrackEvent(file, midiFile.tracks[index], midiFile.state)) {
			return false;
		}
	}
	return true;
}

bool readNextTrackEvent(vfs::FileHandle& file, TrackCursor& track, MidiState const& state) {
	track.hasEvent = false;
	if (track.done || track.position >= track.end) {
		track.done = true;
		return true;
	}
	if (!file.seek(track.position)) {
		return false;
	}

	std::uint32_t delta = 0;
	if (!readVariable(file, track.position, track.end, delta)) {
		return false;
	}
	track.tick += delta;

	std::uint8_t first = 0;
	if (!readU8(file, track.position, track.end, first)) {
		return false;
	}

	std::uint8_t status = first;
	std::uint8_t firstData = 0;
	bool hasFirstData = false;
	if (first < 0x80U) {
		if (track.runningStatus == 0) {
			return false;
		}
		status = track.runningStatus;
		firstData = first;
		hasFirstData = true;
	} else if (first < 0xF0U) {
		track.runningStatus = first;
	}

	MidiEvent event = makeBaseEvent(track, state);

	if (status < 0xF0U) {
		auto const eventClass = static_cast<std::uint8_t>(status & 0xF0U);
		auto const channel = static_cast<std::uint8_t>(status & 0x0FU);
		setEventChannel(event, channel);

		std::uint8_t data1 = 0;
		std::uint8_t data2 = 0;
		if (!readDataByte(file, track.position, track.end, hasFirstData, firstData, data1)) {
			return false;
		}

		if (eventClass != 0xC0U && eventClass != 0xD0U) {
			if (!readDataByte(file, track.position, track.end, hasFirstData, firstData, data2)) {
				return false;
			}
		}

		switch (eventClass) {
			case 0x80U:
				event.type = MidiEventType::NoteOff;
				event.a = data1;
				event.b = data2;
				break;
			case 0x90U:
				event.type = data2 == 0 ? MidiEventType::NoteOff : MidiEventType::NoteOn;
				event.a = data1;
				event.b = data2;
				break;
			case 0xB0U:
				event.type = MidiEventType::ControlChange;
				event.a = data1;
				event.b = data2;
				break;
			case 0xC0U:
				event.type = MidiEventType::ProgramChange;
				event.a = data1;
				break;
			case 0xE0U:
				event.type = MidiEventType::PitchBend;
				event.a = static_cast<std::uint16_t>(data1 | (static_cast<std::uint16_t>(data2) << 7U));
				break;
			case 0xA0U:
			case 0xD0U:
			default:
				event.type = MidiEventType::Other;
				event.a = data1;
				event.b = data2;
				break;
		}

		track.event = event;
		track.hasEvent = true;
		return true;
	}

	if (status == 0xFFU) {
		std::uint8_t metaType = 0;
		std::uint32_t size = 0;
		if (!readU8(file, track.position, track.end, metaType)
		    || !readVariable(file, track.position, track.end, size)) {
			return false;
		}

		if (metaType == 0x2FU) {
			if (!seekForward(file, track.position, track.end, size)) {
				return false;
			}
			event.type = MidiEventType::EndOfTrack;
			track.done = true;
			track.event = event;
			track.hasEvent = true;
			return true;
		}

		if (metaType == 0x51U && size == 3) {
			std::uint8_t bytes[3]{};
			if (!readBytes(file, track.position, track.end, bytes, sizeof(bytes))) {
				return false;
			}
			event.type = MidiEventType::SetTempo;
			event.tempo = (static_cast<std::uint32_t>(bytes[0]) << 16U)
			    | (static_cast<std::uint32_t>(bytes[1]) << 8U)
			    | bytes[2];
		} else if (!seekForward(file, track.position, track.end, size)) {
			return false;
		}

		track.event = event;
		track.hasEvent = true;
		return true;
	}

	if (status == 0xF0U || status == 0xF7U) {
		std::uint32_t size = 0;
		if (!readVariable(file, track.position, track.end, size)
		    || !seekForward(file, track.position, track.end, size)) {
			return false;
		}
		track.event = event;
		track.hasEvent = true;
		return true;
	}

	return false;
}

} // namespace lexxos::apps::music::midi::detail
