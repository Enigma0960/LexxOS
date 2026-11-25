#include <lexxos/apps/music/midi/detail/pcm_output_test.h>

#include <array>
#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/music/midi/detail/midi_playback.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::music::midi::detail {

namespace {

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

} // namespace

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

} // namespace lexxos::apps::music::midi::detail
