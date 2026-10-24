#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

std::uint32_t elapsedMs(std::uint32_t startMs, std::uint32_t endMs) {
	return endMs - startMs;
}

std::uint32_t profileOtherMs(GbEmuProfileWindow const& profile) {
	auto const known = profile.inputMs + profile.cpuMs + profile.ppuMs + profile.blitMs;
	return profile.wallMs > known ? profile.wallMs - known : 0;
}

void writeFixedHundredths(lexxos::os::AppContext& context, std::uint64_t hundredths) {
	writeUnsigned64(context, hundredths / 100U);
	context.write(".");
	auto const fraction = static_cast<std::uint32_t>(hundredths % 100U);
	if (fraction < 10U) {
		context.write("0");
	}
	writeUnsigned(context, fraction);
}

void writeRatePerSecond(lexxos::os::AppContext& context, std::uint32_t count, std::uint32_t milliseconds) {
	if (milliseconds == 0) {
		context.write("0.00");
		return;
	}
	writeFixedHundredths(context, (static_cast<std::uint64_t>(count) * 100000U) / milliseconds);
}

void writeGbEmuProfileLine(
    lexxos::os::AppContext& context,
    char const* label,
    std::uint32_t lastFrame,
    GbEmuProfileWindow const& profile,
    std::uint32_t cyclesPerFrame) {
	if (profile.frames == 0 && profile.steps == 0 && profile.cycles == 0 && profile.wallMs == 0) {
		return;
	}

	context.write("gbemu-profile ");
	context.write(label);
	context.write(": last-frame=");
	writeUnsigned(context, lastFrame);
	if (profile.frames != 0) {
		context.write(" frames=");
		writeUnsigned(context, profile.frames);
		context.write(" rendered=");
		writeUnsigned(context, profile.renderedFrames);
	}
	context.write(" wall-ms=");
	writeUnsigned(context, profile.wallMs);
	context.write(" input-ms=");
	writeUnsigned(context, profile.inputMs);
	context.write(" cpu-ms=");
	writeUnsigned(context, profile.cpuMs);
	context.write(" ppu-ms=");
	writeUnsigned(context, profile.ppuMs);
	context.write(" blit-ms=");
	writeUnsigned(context, profile.blitMs);
	context.write(" other-ms=");
	writeUnsigned(context, profileOtherMs(profile));
	context.write(" steps=");
	writeUnsigned(context, profile.steps);
	context.write(" cycles=");
	writeUnsigned(context, profile.cycles);
	if (profile.frames != 0) {
		context.write(" fps=");
		writeRatePerSecond(context, profile.frames, profile.wallMs);
	}
	if (cyclesPerFrame != 0 && profile.wallMs != 0) {
		context.write(" realtime-x=");
		auto const emulatedMs = (static_cast<std::uint64_t>(profile.cycles) * 1000U) / 4194304U;
		writeFixedHundredths(context, (emulatedMs * 100U) / profile.wallMs);
	}
	context.write(" cache-fill=");
	writeUnsigned(context, profile.cacheFills);
	context.write(" cache-retry=");
	writeUnsigned(context, profile.cacheRetries);
	context.write(" cache-fail=");
	writeUnsigned(context, profile.cacheFailures);
	if (profile.cacheReadBytes != 0) {
		context.write(" cache-kib=");
		writeUnsigned64(context, profile.cacheReadBytes / 1024U);
	}
	if (profile.cacheFillMs != 0) {
		context.write(" cache-ms=");
		writeUnsigned(context, profile.cacheFillMs);
	}
	context.writeLine("");
}

} // namespace lexxos::apps::gameboy_programs
