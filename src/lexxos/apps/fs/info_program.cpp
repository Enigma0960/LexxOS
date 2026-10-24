#include <lexxos/apps/fs/info_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {
namespace {

	using lexxos::os::AppContext;
	using lexxos::vfs::FileHandle;
	using lexxos::vfs::NodeType;

	constexpr std::size_t kManifestLimit = 4096;
	constexpr std::size_t kFooterSize = 12;
	constexpr std::size_t kProbeBytes = 128;
	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using ManifestBuffer = std::array<char, kManifestLimit>;

	constexpr lexxos::program::ProgramArgumentSpec kArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "file or directory path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     }
    };

	void label(AppContext& context, std::string_view key, std::string_view value) {
		context.write(key);
		context.write(": ");
		context.writeLine(value);
	}

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

	// Minimal, bounded JSON reader for the packer's manifest: no heap or STL containers.
	// The first pass validates the complete document; the second prints leaf values.
	class ManifestPrinter final {
	public:
		ManifestPrinter(AppContext& context, std::string_view json, bool emit)
		    : context_(context)
		    , json_(json)
		    , emit_(emit) {
			constexpr std::string_view prefix = "manifest.";
			for (auto ch : prefix) {
				label_[length_++] = ch;
			}
		}

		bool run() {
			spaces();
			if (!object(0)) {
				return false;
			}
			spaces();
			return position_ == json_.size();
		}

	private:
		void spaces() {
			while (position_ < json_.size() && (json_[position_] == ' ' || json_[position_] == '\n' || json_[position_] == '\r' || json_[position_] == '\t')) {
				++position_;
			}
		}

		bool consume(char ch) {
			spaces();
			if (position_ >= json_.size() || json_[position_] != ch) {
				return false;
			}
			++position_;
			return true;
		}

		bool string(std::string_view& value) {
			if (!consume('"')) {
				return false;
			}
			auto const begin = position_;
			while (position_ < json_.size()) {
				char const ch = json_[position_++];
				if (ch == '"') {
					value = json_.substr(begin, position_ - begin - 1);
					return true;
				}
				if (static_cast<unsigned char>(ch) < 0x20U) {
					return false;
				}
				if (ch != '\\') {
					continue;
				}
				if (position_ >= json_.size()) {
					return false;
				}
				char const escape = json_[position_++];
				if (escape == 'u') {
					for (unsigned i = 0; i < 4; ++i) {
						if (position_ >= json_.size()) {
							return false;
						}
						char const digit = json_[position_++];
						if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'))) {
							return false;
						}
					}
				} else if (escape != '"' && escape != '\\' && escape != '/' && escape != 'b' && escape != 'f' && escape != 'n' && escape != 'r' && escape != 't') {
					return false;
				}
			}
			return false;
		}

		bool scalar(bool show) {
			spaces();
			std::string_view data;
			if (position_ >= json_.size()) {
				return false;
			}
			if (json_[position_] == '"') {
				if (!string(data)) {
					return false;
				}
			} else {
				auto const begin = position_;
				while (position_ < json_.size()) {
					char const ch = json_[position_];
					if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
						break;
					}
					++position_;
				}
				if (position_ == begin) {
					return false;
				}
				data = json_.substr(begin, position_ - begin);
				// JSON scalar other than a string: number, boolean or null.
				if (data != "true" && data != "false" && data != "null") {
					std::size_t i = data[0] == '-' ? 1U : 0U;
					bool digit = false;
					for (; i < data.size(); ++i) {
						char const ch = data[i];
						if (ch >= '0' && ch <= '9') {
							digit = true;
						} else if (ch != '.' && ch != 'e' && ch != 'E' && ch != '+' && ch != '-') {
							return false;
						}
					}
					if (!digit) {
						return false;
					}
				}
			}
			if (emit_ && show) {
				context_.write(std::string_view{label_.data(), length_});
				context_.write(": ");
				context_.writeLine(data);
			}
			return true;
		}

		bool value(unsigned depth, bool show) {
			if (depth > 8) {
				return false;
			}
			spaces();
			if (position_ >= json_.size()) {
				return false;
			}
			if (json_[position_] == '{') {
				return object(depth + 1, show);
			}
			if (json_[position_] == '[') {
				if (emit_ && show) {
					context_.write(std::string_view{label_.data(), length_});
					context_.writeLine(": [array]");
				}
				if (!consume('[')) {
					return false;
				}
				spaces();
				if (consume(']')) {
					return true;
				}
				do {
					if (!value(depth + 1, false)) {
						return false;
					}
					spaces();
					if (consume(']')) {
						return true;
					}
				} while (consume(','));
				return false;
			}
			return scalar(show);
		}

		bool object(unsigned depth, bool show = true) {
			if (depth > 8 || !consume('{')) {
				return false;
			}
			spaces();
			if (consume('}')) {
				return true;
			}
			do {
				std::string_view key;
				if (!string(key) || !consume(':')) {
					return false;
				}
				auto const oldLength = length_;
				if (key.size() + length_ + 1 >= label_.size()) {
					return false;
				}
				for (char ch : key) {
					label_[length_++] = ch;
				}
				spaces();
				bool const nested = position_ < json_.size() && json_[position_] == '{';
				if (nested) {
					label_[length_++] = '.';
				}
				if (!value(depth, show)) {
					return false;
				}
				length_ = oldLength;
				spaces();
				if (consume('}')) {
					return true;
				}
			} while (consume(','));
			return false;
		}

		AppContext& context_;
		std::string_view json_;
		bool emit_{false};
		std::size_t position_{0};
		std::array<char, 256> label_{};
		std::size_t length_{0};
	};

	int printLexx(AppContext& context, std::string_view path) {
		FileHandle file{};
		if (context.vfs().open(path, lexxos::vfs::OpenMode::Read, file) != lexxos::vfs::OpenStatus::Ok) {
			label(context, "lexx", "cannot open executable");
			return 1;
		}
		std::array<std::uint8_t, lexxos::external_program::lexxExecutableHeaderSize> header{};
		if (file.size() < header.size() || !readExact(file, header.data(), header.size())) {
			label(context, "lexx", "invalid header");
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
			label(context, "lexx", "invalid header");
			return 1;
		}

		context.write("lexx format: ");
		lexxos::apps::writeUnsigned(context, lexxos::apps::readLe16(header.data() + 4));
		context.writeLine("");
		label(context, "lexx machine", lexxos::apps::readLe16(header.data() + 8) == lexxos::external_program::lexxMachineRv32i ? "RV32I" : "unknown");
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
			label(context, "manifest", "not present (legacy)");
			return 0;
		}
		std::array<std::uint8_t, kFooterSize> footer{};
		if (!file.seek(file.size() - footer.size()) || !readExact(file, footer.data(), footer.size())) {
			label(context, "manifest", "unreadable footer");
			return 1;
		}
		if (std::memcmp(footer.data(), "LXMF", 4) != 0) {
			label(context, "manifest", "not present (legacy)");
			return 0;
		}
		auto const length = lexxos::apps::readLe32(footer.data() + 4);
		auto const checksum = lexxos::apps::readLe32(footer.data() + 8);
		auto const trailer = file.size() - kFooterSize;
		if (length == 0 || length > kManifestLimit || length > trailer || trailer - length < imageEnd) {
			label(context, "manifest", "invalid location or length");
			return 1;
		}

		auto* arena = context.memoryArena();
		auto* storage = arena != nullptr ? arena->allocate(sizeof(ManifestBuffer), alignof(ManifestBuffer)) : nullptr;
		if (storage == nullptr) {
			label(context, "manifest", "not enough workspace memory");
			return 1;
		}
		auto* buffer = new (storage) ManifestBuffer{};
		if (!file.seek(trailer - length) || !readExact(file, buffer->data(), length)) {
			label(context, "manifest", "unreadable data");
			return 1;
		}
		if (crc32(reinterpret_cast<std::uint8_t const*>(buffer->data()), length) != checksum) {
			label(context, "manifest", "CRC32 mismatch");
			return 1;
		}
		std::string_view const json{buffer->data(), length};
		if (!ManifestPrinter{context, json, false}.run()) {
			label(context, "manifest", "invalid JSON");
			return 1;
		}
		label(context, "manifest", "present (CRC32 OK)");
		(void) ManifestPrinter{context, json, true}.run();
		return 0;
	}

} // namespace

lexxos::program::ProgramDescriptor InfoProgram::descriptor() {
	return {
	    .name = "info",
	    .help = "show file or directory information",
	    .usage = "info <path>",
	    .arguments = lexxos::apps::argumentList(kArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(PathBuffer) + sizeof(ManifestBuffer)),
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int InfoProgram::run(AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& args) {
	auto* arena = context.memoryArena();
	if (arena == nullptr) {
		context.writeErrorLine("info: program memory arena is unavailable");
		return 1;
	}
	auto* storage = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
	if (storage == nullptr) {
		context.writeErrorLine("info: not enough workspace memory");
		return 1;
	}
	auto* path = new (storage) PathBuffer{};
	if (!lexxos::apps::resolvePath(context, args.positional("path"), *path)) {
		return 2;
	}

	lexxos::vfs::Stat stat{};
	if (!context.vfs().stat(path->data(), stat)) {
		context.writeError("info: cannot access: ");
		context.writeErrorLine(path->data());
		return 1;
	}
	label(context, "name", baseName(path->data()));
	label(context, "path", path->data());
	if (stat.type == NodeType::Directory) {
		label(context, "type", "directory");
	} else if (stat.type == NodeType::RegularFile) {
		label(context, "type", fileType(context, path->data(), stat.sizeBytes));
	} else {
		label(context, "type", lexxos::vfs::toString(stat.type));
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
		if (!context.vfs().list(path->data(), countEntry, &counts)) {
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
	if (stat.type == NodeType::RegularFile && endsWithIgnoreCase(baseName(path->data()), ".lexx")) {
		return printLexx(context, path->data());
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
