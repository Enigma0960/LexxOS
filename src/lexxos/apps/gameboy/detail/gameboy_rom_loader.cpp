#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

bool readCartridgeHeader(
    lexxos::os::AppContext& context,
    std::string_view programName,
    std::string_view path,
    std::array<std::uint8_t, lexxos::gameboy::cartridgeHeaderSize>& header,
    lexxos::vfs::FileHandle& file) {
	auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write(programName);
		context.write(": ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return false;
	}

	if (!lexxos::gameboy::hasCompleteCartridgeHeader(static_cast<std::size_t>(file.size()))) {
		context.write(programName);
		context.write(": ");
		context.write(path);
		context.writeLine(": file is too small for a Game Boy cartridge header");
		return false;
	}

	std::size_t bytesRead = 0;
	auto const readStatus = file.read(header.data(), header.size(), bytesRead);
	if (readStatus != lexxos::vfs::ReadStatus::Ok) {
		context.write(programName);
		context.write(": ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(readStatus));
		return false;
	}

	if (bytesRead != header.size()) {
		context.write(programName);
		context.write(": ");
		context.write(path);
		context.writeLine(": failed to read full cartridge header");
		return false;
	}

	return true;
}

bool readCartridgeHeader(
    lexxos::os::AppContext& context,
    std::string_view path,
    std::array<std::uint8_t, lexxos::gameboy::cartridgeHeaderSize>& header,
    lexxos::vfs::FileHandle& file) {
	return readCartridgeHeader(context, "gb-info", path, header, file);
}

void writeRomSizeConsistency(lexxos::os::AppContext& context, std::uint64_t actualSize, std::uint32_t expectedSize) {
	if (expectedSize == 0) {
		context.writeLine("rom size check: skipped, unknown size code");
		return;
	}

	context.write("rom size check: ");
	context.writeLine(actualSize == expectedSize ? "ok" : "warning: file size differs from header size");
}

constexpr std::uint32_t fnv1aPrime = 16777619U;

std::uint32_t fnv1aUpdate(std::uint32_t hash, std::uint8_t const* data, std::size_t size) {
	for (std::size_t index = 0; index < size; ++index) {
		hash ^= data[index];
		hash *= fnv1aPrime;
	}
	return hash;
}

bool readFileBlock(lexxos::vfs::FileHandle& file, std::uint32_t offset, std::uint8_t* buffer, std::size_t size) {
	if (!file.seek(offset)) {
		return false;
	}
	std::size_t bytesRead = 0;
	auto const status = file.read(buffer, size, bytesRead);
	return status == lexxos::vfs::ReadStatus::Ok && bytesRead == size;
}

bool readFileBlockRetry(lexxos::vfs::FileHandle& file, std::uint32_t offset, std::uint8_t* buffer, std::size_t size, std::uint8_t attempts) {
	for (std::uint8_t attempt = 0; attempt < attempts; ++attempt) {
		if (readFileBlock(file, offset, buffer, size)) {
			return true;
		}
	}
	return false;
}

void writeRomProbe(lexxos::os::AppContext& context, std::uint32_t offset, std::uint8_t const* data, std::size_t size) {
	context.write(" probe[0x");
	writeHex32(context, offset);
	context.write("]=");
	for (std::size_t index = 0; index < size; ++index) {
		if (index != 0) {
			context.write(".");
		}
		writeCode8(context, data[index]);
	}
}

} // namespace lexxos::apps::gameboy_programs
