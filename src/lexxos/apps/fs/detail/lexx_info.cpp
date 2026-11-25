#include <lexxos/apps/fs/detail/lexx_info.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/info_output.h>
#include <lexxos/apps/fs/detail/manifest_printer.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/program/program_memory_arena.h>

namespace lexxos::apps::filesystem_programs::detail {

using os::AppContext;
using vfs::FileHandle;

namespace {

	constexpr std::size_t kManifestLimit = lexxInfoWorkspaceBytes;
	constexpr std::size_t kFooterSize = 12;
	using ManifestBuffer = std::array<char, kManifestLimit>;

	bool readExact(FileHandle& file, void* destination, std::size_t count) {
		auto* bytes = static_cast<std::uint8_t*>(destination);
		std::size_t done = 0;

		while (done < count) {
			std::size_t actual = 0;
			if (file.read(bytes + done, count - done, actual) != lexxos::vfs::ReadStatus::Ok || actual == 0) {
				return false;
			}

			done += actual;
		}

		return true;
	}

	std::uint32_t crc32(std::uint8_t const* bytes, std::size_t size) {
		std::uint32_t crc = 0xFFFFFFFFU;

		for (std::size_t i = 0; i < size; ++i) {
			crc ^= bytes[i];
			for (unsigned bit = 0; bit < 8; ++bit) {
				crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
			}
		}

		return crc ^ 0xFFFFFFFFU;
	}

} // namespace

int inspectLexxInfo(AppContext& context, std::string_view path) {
	FileHandle file{};
	if (context.vfs().open(path, lexxos::vfs::OpenMode::Read, file) != lexxos::vfs::OpenStatus::Ok) {
		writeInfoLabel(context, "lexx", "cannot open executable");
		return 1;
	}

	std::array<std::uint8_t, lexxos::external_program::lexxExecutableHeaderSize> header{};
	if (file.size() < header.size() || !readExact(file, header.data(), header.size())) {
		writeInfoLabel(context, "lexx", "invalid header");
		return 1;
	}

	auto const imageOffset = lexxos::apps::readLe32(header.data() + 24);
	auto const imageSize = lexxos::apps::readLe32(header.data() + 28);
	std::uint64_t const imageEnd = static_cast<std::uint64_t>(imageOffset) + imageSize;

	if (lexxos::apps::readLe32(header.data()) != lexxos::external_program::lexxExecutableMagic
	    || lexxos::apps::readLe16(header.data() + 6) != header.size()
	    || imageOffset < header.size()
	    || imageSize == 0
	    || imageEnd > file.size()) {
		writeInfoLabel(context, "lexx", "invalid header");
		return 1;
	}

	context.write("lexx format: ");
	lexxos::apps::writeUnsigned(context, lexxos::apps::readLe16(header.data() + 4));
	context.writeLine("");
	writeInfoLabel(context, "lexx machine", lexxos::apps::readLe16(header.data() + 8) == lexxos::external_program::lexxMachineRv32i ? "RV32I" : "unknown");
	context.write("lexx ABI: ");
	lexxos::apps::writeUnsigned(context, lexxos::apps::readLe16(header.data() + 10));
	context.writeLine("");
	context.write("lexx capabilities: ");
	lexxos::apps::writeHex32(context, lexxos::apps::readLe32(header.data() + 16));
	context.writeLine("");
	context.write("lexx image bytes: ");
	lexxos::apps::writeUnsigned(context, imageSize);
	context.writeLine("");
	context.write("lexx memory bytes: ");
	lexxos::apps::writeUnsigned(context, lexxos::apps::readLe32(header.data() + 32));
	context.writeLine("");
	context.write("lexx stack bytes: ");
	lexxos::apps::writeUnsigned(context, lexxos::apps::readLe32(header.data() + 36));
	context.writeLine("");

	if (file.size() < imageEnd + kFooterSize) {
		writeInfoLabel(context, "manifest", "not present (legacy)");
		return 0;
	}

	std::array<std::uint8_t, kFooterSize> footer{};
	if (!file.seek(file.size() - footer.size()) || !readExact(file, footer.data(), footer.size())) {
		writeInfoLabel(context, "manifest", "unreadable footer");
		return 1;
	}

	if (std::memcmp(footer.data(), "LXMF", 4) != 0) {
		writeInfoLabel(context, "manifest", "not present (legacy)");
		return 0;
	}

	auto const length = lexxos::apps::readLe32(footer.data() + 4);
	auto const checksum = lexxos::apps::readLe32(footer.data() + 8);
	auto const trailer = file.size() - kFooterSize;
	if (length == 0 || length > kManifestLimit || length > trailer || trailer - length < imageEnd) {
		writeInfoLabel(context, "manifest", "invalid location or length");
		return 1;
	}

	auto* arena = context.memoryArena();
	auto* storage = arena != nullptr ? arena->allocate(sizeof(ManifestBuffer), alignof(ManifestBuffer)) : nullptr;
	if (storage == nullptr) {
		writeInfoLabel(context, "manifest", "not enough workspace memory");
		return 1;
	}

	auto* buffer = new (storage) ManifestBuffer{};
	if (!file.seek(trailer - length) || !readExact(file, buffer->data(), length)) {
		writeInfoLabel(context, "manifest", "unreadable data");
		return 1;
	}

	if (crc32(reinterpret_cast<std::uint8_t const*>(buffer->data()), length) != checksum) {
		writeInfoLabel(context, "manifest", "CRC32 mismatch");
		return 1;
	}

	std::string_view const json{buffer->data(), length};
	if (!ManifestPrinter{context, json, false}.run()) {
		writeInfoLabel(context, "manifest", "invalid JSON");
		return 1;
	}

	writeInfoLabel(context, "manifest", "present (CRC32 OK)");
	(void) ManifestPrinter{context, json, true}.run();
	return 0;
}

} // namespace lexxos::apps::filesystem_programs::detail
