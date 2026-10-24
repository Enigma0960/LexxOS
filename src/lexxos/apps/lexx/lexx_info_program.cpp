#include <lexxos/apps/lexx/lexx_info_program.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::lexx_programs {
namespace {

	constexpr std::uint32_t kManifestFooterSize = 12;
	constexpr std::uint32_t kMaximumManifestBytes = 4096;
	constexpr std::size_t kIoBufferSize = 128;

	bool readExact(lexxos::vfs::FileHandle& file, void* destination, std::size_t bytes) {
		auto* output = static_cast<std::uint8_t*>(destination);
		std::size_t completed = 0;
		while (completed < bytes) {
			std::size_t received = 0;
			if (file.read(output + completed, bytes - completed, received) != lexxos::vfs::ReadStatus::Ok || received == 0) {
				return false;
			}
			completed += received;
		}
		return true;
	}

	std::uint32_t updateCrc32(std::uint32_t crc, std::uint8_t const* data, std::size_t size) {
		for (std::size_t index = 0; index < size; ++index) {
			crc ^= data[index];
			for (unsigned bit = 0; bit < 8; ++bit) {
				crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
			}
		}
		return crc;
	}

	// Streaming avoids loading JSON into a guest arena or into a large stack buffer.
	bool streamManifest(lexxos::os::AppContext& context, lexxos::vfs::FileHandle& file,
	    std::uint64_t offset, std::uint32_t length, std::uint32_t expectedCrc, bool print) {
		if (!file.seek(offset)) {
			return false;
		}
		std::array<std::uint8_t, kIoBufferSize> buffer{};
		std::uint32_t remaining = length;
		std::uint32_t crc = 0xFFFFFFFFU;
		while (remaining != 0U) {
			auto const count = static_cast<std::size_t>(std::min<std::uint32_t>(remaining, buffer.size()));
			if (!readExact(file, buffer.data(), count)) {
				return false;
			}
			for (std::size_t index = 0; index < count; ++index) {
				auto const character = buffer[index];
				if (character < 0x20U && character != '\r' && character != '\n' && character != '\t') {
					return false;
				}
			}
			crc = updateCrc32(crc, buffer.data(), count);
			if (print) {
				context.write(std::string_view{reinterpret_cast<char const*>(buffer.data()), count});
			}
			remaining -= static_cast<std::uint32_t>(count);
		}
		return (crc ^ 0xFFFFFFFFU) == expectedCrc;
	}

	void printHeader(lexxos::os::AppContext& context, std::string_view path,
	    std::uint8_t const* bytes, std::uint64_t fileSize) {
		context.write("file: ");
		context.writeLine(path);
		context.write("format: LEXX/");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe16(bytes + 4));
		context.writeLine("");
		context.write("machine: ");
		context.writeLine(lexxos::apps::readLe16(bytes + 8) == lexxos::external_program::lexxMachineRv32i ? "RV32I" : "unknown");
		context.write("abi: ");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe16(bytes + 10));
		context.writeLine("");
		context.write("capabilities: ");
		lexxos::apps::writeHex32(context, lexxos::apps::readLe32(bytes + 16));
		context.writeLine("");
		context.write("image bytes: ");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe32(bytes + 28));
		context.writeLine("");
		context.write("memory bytes: ");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe32(bytes + 32));
		context.writeLine("");
		context.write("stack bytes: ");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe32(bytes + 36));
		context.writeLine("");
		context.write("file bytes: ");
		lexxos::apps::writeUnsigned64(context, fileSize);
		context.writeLine("");
	}

	void printUsage(lexxos::os::AppContext& context) {
		context.writeErrorLine("usage: lexx-info [--json] <file.lexx>");
	}

} // namespace

lexxos::program::ProgramDescriptor LexxInfoProgram::descriptor() {
	return {
	    .name = "lexx-info",
	    .help = "inspect an external .lexx file without running it",
	    .description = "Shows an external executable's header and embedded version, author, license and build timestamp.",
	    .usage = "lexx-info [--json] <file.lexx>",
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int LexxInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argv == nullptr) {
		printUsage(context);
		return 2;
	}
	bool jsonOnly = false;
	int pathIndex = 1;
	if (invocation.argc > 1 && std::string_view{invocation.argv[1]} == "--json") {
		jsonOnly = true;
		pathIndex = 2;
	}
	if (invocation.argc != pathIndex + 1) {
		printUsage(context);
		return 2;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!lexxos::apps::resolvePath(context, invocation.argv[pathIndex], path)) {
		context.writeErrorLine("lexx-info: invalid path");
		return 2;
	}

	lexxos::vfs::FileHandle file{};
	auto const status = context.vfs().open(path.data(), lexxos::vfs::OpenMode::Read, file);
	if (status != lexxos::vfs::OpenStatus::Ok || !file.isOpen()) {
		context.writeError("lexx-info: open failed: ");
		context.writeErrorLine(lexxos::vfs::toString(status));
		return 1;
	}

	std::array<std::uint8_t, lexxos::external_program::lexxExecutableHeaderSize> header{};
	if (file.size() < header.size() || !readExact(file, header.data(), header.size())) {
		context.writeErrorLine("lexx-info: short or unreadable header");
		return 1;
	}
	auto const imageOffset = lexxos::apps::readLe32(header.data() + 24);
	auto const imageSize = lexxos::apps::readLe32(header.data() + 28);
	auto const imageEnd = static_cast<std::uint64_t>(imageOffset) + imageSize;
	if (lexxos::apps::readLe32(header.data()) != lexxos::external_program::lexxExecutableMagic
	    || lexxos::apps::readLe16(header.data() + 6) != header.size()
	    || imageOffset < header.size()
	    || imageSize == 0
	    || imageEnd > file.size()) {
		context.writeErrorLine("lexx-info: invalid .lexx file/header");
		return 1;
	}
	if (!jsonOnly) {
		printHeader(context, path.data(), header.data(), file.size());
	}

	// v1 loader permits a trailer after the executable image. No ABI update.
	if (file.size() < imageEnd + kManifestFooterSize) {
		if (jsonOnly) {
			context.writeErrorLine("lexx-info: this file has no embedded manifest");
			return 1;
		}
		context.writeLine("manifest: not present (legacy)");
		return 0;
	}
	std::array<std::uint8_t, kManifestFooterSize> footer{};
	if (!file.seek(file.size() - footer.size()) || !readExact(file, footer.data(), footer.size())) {
		context.writeErrorLine("lexx-info: failed to read manifest footer");
		return 1;
	}
	if (footer[0] != 'L' || footer[1] != 'X' || footer[2] != 'M' || footer[3] != 'F') {
		if (jsonOnly) {
			context.writeErrorLine("lexx-info: this file has no embedded manifest");
			return 1;
		}
		context.writeLine("manifest: not present (legacy)");
		return 0;
	}
	auto const length = lexxos::apps::readLe32(footer.data() + 4);
	auto const checksum = lexxos::apps::readLe32(footer.data() + 8);
	auto const trailerStart = file.size() - footer.size();
	if (length == 0 || length > kMaximumManifestBytes || length > trailerStart || trailerStart - length < imageEnd) {
		context.writeErrorLine("lexx-info: invalid manifest location or size");
		return 1;
	}
	auto const manifestStart = trailerStart - length;
	if (!streamManifest(context, file, manifestStart, length, checksum, false)) {
		context.writeErrorLine("lexx-info: manifest checksum/read failed");
		return 1;
	}
	if (!jsonOnly) {
		context.writeLine("manifest:");
	}
	if (!streamManifest(context, file, manifestStart, length, checksum, true)) {
		context.writeErrorLine("lexx-info: manifest read failed");
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::lexx_programs
