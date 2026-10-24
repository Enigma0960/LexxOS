#include <lexxos/apps/gameboy/gb_rom_check_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbRomCheckProgram::descriptor() {
	return {
	    .name = "gb-rom-check",
	    .help = "verify repeated Game Boy ROM reads",
	    .description = "Reads stable probe blocks from a ROM file and optionally hashes the full image. This helps separate CPU/PPU/display issues from SD/FAT/ROM-read instability.",
	    .usage = "gb-rom-check [--passes count] [--full] <rom>",
	    .arguments = argumentList(gbRomCheckArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbRomCheckProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t passes = 2;
	if (!parseUnsigned(arguments.valueOr("passes", "2"), passes) || passes == 0) {
		context.writeErrorLine("gb-rom-check: invalid pass count");
		return 2;
	}
	if (passes > 8) {
		passes = 8;
	}

	auto const full = arguments.has("full");

	std::uint32_t previousFullHash = 0;
	std::uint32_t previousSampleHash = 0;
	bool stable = true;
	bool allReadsOk = true;
	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::Read, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			context.writeErrorLine("gb-rom-check: failed to open ROM");
			return 1;
		}

		auto const fileSize64 = file.size();
		auto const fileSize = static_cast<std::uint32_t>(fileSize64 > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : fileSize64);
		std::array<std::uint8_t, 512> block{};
		std::uint32_t fullHash = fnv1aOffsetBasis;
		std::uint32_t failedOffset = 0;
		bool fullReadOk = true;
		if (full) {
			std::uint32_t offset = 0;
			while (offset < fileSize) {
				auto const remaining = fileSize - offset;
				auto const chunk = static_cast<std::size_t>(remaining < block.size() ? remaining : block.size());
				if (!readFileBlockRetry(file, offset, block.data(), chunk, 4)) {
					fullReadOk = false;
					failedOffset = offset;
					break;
				}
				fullHash = fnv1aUpdate(fullHash, block.data(), chunk);
				offset += static_cast<std::uint32_t>(chunk);
			}
		}

		std::uint32_t sampleHash = fnv1aOffsetBasis;
		std::uint8_t value0038 = 0xFF;
		std::uint8_t value0100 = 0xFF;
		std::uint8_t value0147 = 0xFF;
		std::uint8_t value0148 = 0xFF;
		std::uint8_t value0149 = 0xFF;
		bool sampleReadOk = true;
		std::array<std::uint32_t, 22> offsets{
		    0x0000U,
		    0x0038U,
		    0x0100U,
		    0x0140U,
		    0x0147U,
		    0x1B20U,
		    0x1B2FU,
		    0x1B30U,
		    0x1E20U,
		    0x1E24U,
		    0x1E27U,
		    0x1EF4U,
		    0x1F53U,
		    0x1F62U,
		    0x3E70U,
		    0x3E72U,
		    0x4000U,
		    0x20000U,
		    0x70000U,
		    0x7230FU,
		    0x72315U,
		    0x78000U,
		};
		for (auto const sampleOffset : offsets) {
			if (sampleOffset >= fileSize) {
				continue;
			}
			auto const remaining = fileSize - sampleOffset;
			auto const chunk = static_cast<std::size_t>(remaining < block.size() ? remaining : block.size());
			if (!readFileBlockRetry(file, sampleOffset, block.data(), chunk, 6)) {
				sampleReadOk = false;
				if (failedOffset == 0) {
					failedOffset = sampleOffset;
				}
				continue;
			}
			sampleHash = fnv1aUpdate(sampleHash, block.data(), chunk);
		}
		if (readFileBlockRetry(file, 0x0038U, block.data(), 1, 6)) {
			value0038 = block[0];
		} else {
			sampleReadOk = false;
		}
		if (readFileBlockRetry(file, 0x0100U, block.data(), 1, 6)) {
			value0100 = block[0];
		} else {
			sampleReadOk = false;
		}
		if (readFileBlockRetry(file, 0x0147U, block.data(), 3, 6)) {
			value0147 = block[0];
			value0148 = block[1];
			value0149 = block[2];
		} else {
			sampleReadOk = false;
		}

		context.write("gb-rom-check: pass=");
		writeUnsigned(context, pass + 1);
		context.write(" size=");
		writeUnsigned(context, fileSize);
		context.write(" sample=");
		context.write(sampleReadOk ? "ok" : "failed");
		if (full) {
			context.write(" full=");
			context.write(fullReadOk ? "ok" : "failed");
			if (!fullReadOk) {
				context.write(" failed-offset=0x");
				writeHex32(context, failedOffset);
			}
		}
		context.write(" full-hash=0x");
		writeHex32(context, fullHash);
		context.write(" sample-hash=0x");
		writeHex32(context, sampleHash);
		context.write(" [0038]=");
		writeCode8(context, value0038);
		context.write(" [0100]=");
		writeCode8(context, value0100);
		context.write(" type=");
		writeCode8(context, value0147);
		context.write(" rom-size=");
		writeCode8(context, value0148);
		context.write(" ram-size=");
		writeCode8(context, value0149);
		context.writeLine("");

		std::array<std::uint32_t, 9> byteProbeOffsets{0x1B20U, 0x1B2FU, 0x1B30U, 0x1E24U, 0x1E27U, 0x1F53U, 0x3E70U, 0x3E72U, 0x7230FU};
		context.write("gb-rom-check: pass=");
		writeUnsigned(context, pass + 1);
		for (auto const probeOffset : byteProbeOffsets) {
			std::array<std::uint8_t, 8> bytes{};
			if (probeOffset + bytes.size() <= fileSize && readFileBlockRetry(file, probeOffset, bytes.data(), bytes.size(), 6)) {
				writeRomProbe(context, probeOffset, bytes.data(), bytes.size());
			} else {
				context.write(" probe[0x");
				writeHex32(context, probeOffset);
				context.write("]=failed");
				sampleReadOk = false;
			}
		}
		context.writeLine("");
		if (pass > 0 && (sampleHash != previousSampleHash || (full && fullHash != previousFullHash))) {
			stable = false;
		}
		previousFullHash = fullHash;
		previousSampleHash = sampleHash;
		allReadsOk = allReadsOk && sampleReadOk && (!full || fullReadOk);
	}

	context.write("gb-rom-check: result=");
	if (!allReadsOk) {
		context.writeLine("read-error");
		return 1;
	}
	context.writeLine(stable ? "stable" : "unstable");
	return stable ? 0 : 1;
}

} // namespace lexxos::apps::gameboy_programs
