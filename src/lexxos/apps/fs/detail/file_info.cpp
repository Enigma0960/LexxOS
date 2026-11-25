#include <lexxos/apps/fs/detail/file_info.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/info_output.h>
#include <lexxos/apps/fs/detail/shortcut_info.h>
#include <lexxos/shortcuts/shortcut.h>

namespace lexxos::apps::filesystem_programs::detail {

using os::AppContext;
using vfs::FileHandle;
using vfs::NodeType;

namespace {

	constexpr std::size_t kProbeBytes = 128;

	std::string_view baseName(std::string_view path) {
		if (path == "/") {
			return path;
		}

		while (path.size() > 1 && path.back() == '/') {
			path.remove_suffix(1);
		}

		auto const separator = path.find_last_of('/');
		return separator == std::string_view::npos ? path : path.substr(separator + 1);
	}

	bool endsWithIgnoreCase(std::string_view value, std::string_view extension) {
		if (value.size() < extension.size()) {
			return false;
		}

		value.remove_prefix(value.size() - extension.size());
		for (std::size_t i = 0; i < extension.size(); ++i) {
			char ch = value[i];
			if (ch >= 'A' && ch <= 'Z') {
				ch = static_cast<char>(ch + ('a' - 'A'));
			}

			if (ch != extension[i]) {
				return false;
			}
		}

		return true;
	}

	bool hasExtension(std::string_view name, std::initializer_list<std::string_view> extensions) {
		for (auto extension : extensions) {
			if (endsWithIgnoreCase(name, extension)) {
				return true;
			}
		}

		return false;
	}

	bool startsWith(std::uint8_t const* bytes, std::size_t size, char const* signature, std::size_t length) {
		return size >= length && std::memcmp(bytes, signature, length) == 0;
	}

	std::string_view fileType(AppContext& context, std::string_view path, std::uint64_t size) {
		// Virtual text files often report size 0 even when they generate content.
		if (path.starts_with("/proc/") || path.starts_with("/sys/")) {
			return "text (virtual)";
		}

		std::array<std::uint8_t, kProbeBytes> prefix{};
		std::size_t received = 0;
		FileHandle file{};
		if (context.vfs().open(path, lexxos::vfs::OpenMode::Read, file) == lexxos::vfs::OpenStatus::Ok) {
			if (file.read(prefix.data(), prefix.size(), received) != lexxos::vfs::ReadStatus::Ok) {
				received = 0;
			}
		}

		if (startsWith(prefix.data(), received, "LEXX", 4)) {
			return "LEXX executable";
		}

		if (received >= 4 && prefix[0] == 0x7F && startsWith(prefix.data() + 1, received - 1, "ELF", 3)) {
			return "ELF executable";
		}

		if (received >= 8 && startsWith(prefix.data(), received, "\x89PNG\r\n\x1a\n", 8)) {
			return "PNG image";
		}

		if (received >= 3 && prefix[0] == 0xFF && prefix[1] == 0xD8 && prefix[2] == 0xFF) {
			return "JPEG image";
		}

		if (startsWith(prefix.data(), received, "GIF87a", 6) || startsWith(prefix.data(), received, "GIF89a", 6)) {
			return "GIF image";
		}

		if (startsWith(prefix.data(), received, "BM", 2)) {
			return "BMP image";
		}

		if (startsWith(prefix.data(), received, "%PDF-", 5)) {
			return "PDF document";
		}

		if (startsWith(prefix.data(), received, "PK\x03\x04", 4)) {
			return "ZIP archive";
		}

		auto const name = baseName(path);
		if (hasExtension(name, {".lexx"})) {
			return "LEXX executable (unverified)";
		}

		if (hasExtension(name, {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".ico"})) {
			return "image";
		}

		if (hasExtension(name, {".mp3", ".wav", ".ogg", ".flac", ".mid", ".midi"})) {
			return "audio";
		}

		if (hasExtension(name, {".mp4", ".mkv", ".avi", ".webm"})) {
			return "video";
		}

		if (hasExtension(name, {".zip", ".7z", ".tar", ".gz", ".rar"})) {
			return "archive";
		}

		if (hasExtension(name, {".bin", ".elf", ".o", ".a"})) {
			return "binary";
		}

		// Contents take precedence over the extension for readable files.
		if (received != 0) {
			bool printable = true;
			for (std::size_t i = 0; i < received; ++i) {
				auto ch = prefix[i];
				if (ch == 0 || (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t') || ch == 0x7F) {
					printable = false;
					break;
				}
			}

			return printable ? "text" : "binary";
		}

		if (size == 0) {
			return "empty file";
		}

		if (hasExtension(name, {".txt", ".md", ".json", ".xml", ".yaml", ".yml", ".csv", ".log", ".ini", ".cfg", ".conf", ".c", ".cpp", ".h", ".hpp", ".py", ".sh"})) {
			return "text (unverified)";
		}

		return "unknown";
	}

	void twoDigits(AppContext& context, unsigned value) {
		char digits[2]{static_cast<char>('0' + value / 10U), static_cast<char>('0' + value % 10U)};
		context.write(std::string_view{digits, sizeof(digits)});
	}

	void timestamp(AppContext& context, std::string_view name, lexxos::vfs::FileTimestamp const& time) {
		context.write(name);
		context.write(": ");
		if (time.valid) {
			lexxos::apps::writeUnsigned(context, time.year);
			context.write("-");
			twoDigits(context, time.month);
			context.write("-");
			twoDigits(context, time.day);
			context.write(" ");
			twoDigits(context, time.hour);
			context.write(":");
			twoDigits(context, time.minute);
			context.write(":");
			twoDigits(context, time.second);
		}

		context.writeLine("");
	}

	void permissions(AppContext& context, lexxos::vfs::Stat const& stat) {
		context.write("permissions: ");
		if (stat.hasFatAttributes) {
			context.write(stat.fatAttributes & 0x01U ? "read-only" : "read/write");
		}

		context.writeLine("");
		if (!stat.hasFatAttributes) {
			return;
		}

		context.write("fat attributes: ");
		bool first = true;

		auto emit = [&](char const* name) {
			if (!first) {
				context.write(", ");
			}
			context.write(name);
			first = false;
		};

		if (stat.fatAttributes & 0x01U) {
			emit("read-only");
		}

		if (stat.fatAttributes & 0x02U) {
			emit("hidden");
		}

		if (stat.fatAttributes & 0x04U) {
			emit("system");
		}

		if (stat.fatAttributes & 0x10U) {
			emit("directory");
		}

		if (stat.fatAttributes & 0x20U) {
			emit("archive");
		}

		if (first) {
			context.write("none");
		}

		context.writeLine("");
	}

	struct DirectoryCount {
		AppContext* context{};
		std::uint64_t files{0};
		std::uint64_t directories{0};
	};

	bool countEntry(lexxos::vfs::DirectoryEntry const& entry, void* data) {
		auto& count = *static_cast<DirectoryCount*>(data);
		if (count.context->cancellationRequested()) {
			return false;
		}

		if (entry.name == "." || entry.name == "..") {
			return true;
		}

		if (entry.type == NodeType::Directory) {
			++count.directories;
		} else if (entry.type == NodeType::RegularFile) {
			++count.files;
		}

		return true;
	}

} // namespace

bool isLexxInfoPath(std::string_view path) {
	return endsWithIgnoreCase(baseName(path), ".lexx");
}

int inspectFileInfo(AppContext& context, std::string_view path, vfs::Stat const& stat) {
	writeInfoLabel(context, "name", baseName(path));
	writeInfoLabel(context, "path", path);
	bool invalidShortcut = false;

	if (stat.type == NodeType::Directory) {
		writeInfoLabel(context, "type", "directory");
	} else if (stat.type == NodeType::RegularFile) {
		if (lexxos::shortcuts::isShortcut(path)) {
			invalidShortcut = inspectShortcutInfo(context, path) != 0;
		} else {
			writeInfoLabel(context, "type", fileType(context, path, stat.sizeBytes));
		}
	} else {
		writeInfoLabel(context, "type", lexxos::vfs::toString(stat.type));
	}

	if (stat.type == NodeType::RegularFile) {
		context.write("size: ");
		lexxos::apps::writeUnsigned64(context, stat.sizeBytes);
		context.writeLine(" bytes");
	}

	timestamp(context, "created", stat.createdAt);
	timestamp(context, "modified", stat.modifiedAt);

	if (stat.type == NodeType::Directory) {
		DirectoryCount counts{.context = &context};
		if (!context.vfs().list(path, countEntry, &counts)) {
			context.writeErrorLine(context.cancellationRequested() ? "info: cancelled" : "info: cannot list directory");
			return 1;
		}

		context.write("files: ");
		lexxos::apps::writeUnsigned64(context, counts.files);
		context.writeLine("");
		context.write("directories: ");
		lexxos::apps::writeUnsigned64(context, counts.directories);
		context.writeLine("");
	}

	permissions(context, stat);

	return invalidShortcut ? 1 : 0;
}

} // namespace lexxos::apps::filesystem_programs::detail
