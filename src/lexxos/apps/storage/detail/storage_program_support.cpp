#include <lexxos/apps/storage/detail/storage_program_support.h>

#include <cstring>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	[[nodiscard]] char patternByte(std::uint32_t seed, std::uint64_t offset) {
		return static_cast<char>(0x20U + static_cast<std::uint8_t>((seed + offset) % 95ULL));
	}

	[[nodiscard]] bool appendDecimal(std::array<char, lexxos::vfs::maxPathLength>& path, std::size_t& offset, std::uint32_t value) {
		char digits[10]{};
		std::size_t count = 0;
		if (value == 0) {
			digits[count++] = '0';
		} else {
			while (value != 0 && count < sizeof(digits)) {
				digits[count++] = static_cast<char>('0' + value % 10U);
				value /= 10U;
			}
		}

		while (count > 0) {
			if (offset + 1 >= path.size()) {
				return false;
			}
			path[offset++] = digits[--count];
		}
		path[offset] = '\0';
		return true;
	}

} // namespace

std::uint32_t sdSpeedReferenceBaudRate() {
	return sdStableBaudRate();
}

bool parseOptionalCount(lexxos::os::AppContext& context, std::string_view text, std::uint32_t fallback,
    std::uint32_t maximum, char const* name, std::uint32_t& output) {
	if (text.empty()) {
		output = fallback;
		return true;
	}

	std::uint32_t value = 0;
	if (!parseUnsigned32(text, value) || value == 0 || value > maximum) {
		context.write(name);
		context.write(": expected 1..");
		writeUnsigned(context, maximum);
		context.writeLine("");
		return false;
	}

	output = value;
	return true;
}

std::uint32_t elapsedMs(std::uint32_t startMs, std::uint32_t endMs) {
	std::uint32_t const elapsed = endMs - startMs;
	return elapsed == 0 ? 1 : elapsed;
}

void writeThroughput(lexxos::os::AppContext& context, std::uint64_t bytes, std::uint32_t milliseconds) {
	std::uint64_t const kibPerSecond = (bytes * 1000ULL) / (static_cast<std::uint64_t>(milliseconds) * 1024ULL);
	writeUnsigned64(context, kibPerSecond);
	context.write(" KiB/s");
}

void fillPattern(std::array<char, storageBlockSize>& buffer, std::uint32_t seed, std::uint64_t offset) {
	for (std::size_t index = 0; index < buffer.size(); ++index) {
		buffer[index] = patternByte(seed, offset + index);
	}
}

bool verifyPattern(std::array<char, storageBlockSize> const& buffer, std::size_t size, std::uint32_t seed, std::uint64_t offset) {
	for (std::size_t index = 0; index < size; ++index) {
		if (buffer[index] != patternByte(seed, offset + index)) {
			return false;
		}
	}
	return true;
}

bool makeStressPath(std::uint32_t pass, std::uint32_t file, std::array<char, lexxos::vfs::maxPathLength>& path) {
	constexpr std::string_view prefix = "/mnt/sd/LXS";
	constexpr std::string_view separator = "_";
	constexpr std::string_view suffix = ".TXT";

	std::size_t offset = 0;
	auto append = [&](std::string_view text) -> bool {
		if (offset + text.size() + 1 > path.size()) {
			return false;
		}
		std::memcpy(path.data() + offset, text.data(), text.size());
		offset += text.size();
		path[offset] = '\0';
		return true;
	};

	return append(prefix) && appendDecimal(path, offset, pass) && append(separator) && appendDecimal(path, offset, file) && append(suffix);
}

bool makeSpeedPath(std::uint32_t file, std::array<char, lexxos::vfs::maxPathLength>& path) {
	constexpr std::string_view prefix = "/mnt/sd/LXSPD";
	constexpr std::string_view suffix = ".BIN";

	std::size_t offset = 0;
	auto append = [&](std::string_view text) -> bool {
		if (offset + text.size() + 1 > path.size()) {
			return false;
		}
		std::memcpy(path.data() + offset, text.data(), text.size());
		offset += text.size();
		path[offset] = '\0';
		return true;
	};

	return append(prefix) && appendDecimal(path, offset, file) && append(suffix);
}

std::uint32_t hashBytes(std::uint32_t hash, void const* data, std::size_t size) {
	auto const* bytes = static_cast<std::uint8_t const*>(data);
	for (std::size_t index = 0; index < size; ++index) {
		hash ^= bytes[index];
		hash *= 16777619U;
	}
	return hash;
}

void cleanupSpeedFiles(std::uint32_t files) {
	for (std::uint32_t fileIndex = 0; fileIndex < files; ++fileIndex) {
		std::array<char, lexxos::vfs::maxPathLength> path{};
		if (makeSpeedPath(fileIndex, path)) {
			(void) lexxos::vfs::remove(path.data());
		}
	}
}

} // namespace lexxos::apps::storage_programs::detail
