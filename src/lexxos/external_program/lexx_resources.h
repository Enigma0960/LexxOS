#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::external_program::resources {

// Формат LXRS использует последовательный индекс. LXR2 добавляет относительные
// смещения u32 после записей переменной длины для поиска за O(log N) в .lexx на SD.
// Содержимое ресурсов не загружается целиком в область гостевой VM размером 64 KiB.
inline constexpr std::uint32_t maximumEntries = 65535;
inline constexpr std::uint32_t maximumTableBytes = 8U * 1024U * 1024U;
inline constexpr std::size_t maximumAliasBytes = 127;
inline constexpr std::size_t maximumComponentBytes = 63;
inline constexpr std::uint32_t footerBytes = 16;

struct Index {
	std::uint64_t tableStart{0};
	std::uint64_t tableEnd{0}; // Конец записей переменной длины, без таблицы смещений LXR2.
	std::uint64_t offsetsStart{0};
	std::uint64_t imageEnd{0};
	std::uint32_t count{0};
	bool present{false};
	bool indexed{false};
};

struct Entry {
	std::array<char, maximumAliasBytes + 1> alias{};
	std::uint32_t offset{0};
	std::uint32_t size{0};
};

enum class InspectStatus : std::uint8_t {
	None,
	Valid,
	Invalid
};

inline constexpr std::uint16_t read16(std::uint8_t const* bytes) {
	return static_cast<std::uint16_t>(bytes[0]) | (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

inline constexpr std::uint32_t read32(std::uint8_t const* bytes) {
	return static_cast<std::uint32_t>(bytes[0])
	    | (static_cast<std::uint32_t>(bytes[1]) << 8U)
	    | (static_cast<std::uint32_t>(bytes[2]) << 16U)
	    | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool readExact(lexxos::vfs::FileHandle& file, void* output, std::size_t bytes);

bool validAlias(std::string_view alias);

bool readEntry(lexxos::vfs::FileHandle& file, Index const& index,
    std::uint64_t& cursor, Entry& entry);

// Не выделяет память, не загружает данные ресурсов и не читает исполняемый образ.
InspectStatus inspect(lexxos::vfs::FileHandle& file, std::uint64_t imageEnd, Index& result);

bool find(lexxos::vfs::FileHandle& file, Index const& index,
    std::string_view alias, Entry& result);

template <typename Visitor>
bool enumerate(lexxos::vfs::FileHandle& file, Index const& index, Visitor&& visitor) {
	if (!index.present) {
		return true;
	}

	std::uint64_t cursor = index.tableStart;
	for (std::uint32_t n = 0; n < index.count; ++n) {
		Entry entry{};
		if (!readEntry(file, index, cursor, entry) || !visitor(entry)) {
			return false;
		}
	}

	return cursor == index.tableEnd;
}

enum class PathKind : std::uint8_t {
	Missing,
	File,
	Directory
};

// Каталог ресурсов существует неявно, если содержит хотя бы один псевдоним.
// Корень существует всегда, в том числе у приложений без встроенных ресурсов.
PathKind lookup(lexxos::vfs::FileHandle& file, Index const& index,
    std::string_view path, Entry& output);

// Выдаёт по одной записи на каждого непосредственного потомка каталога.
// Индекс отсортирован по псевдонимам, поэтому одинаковые имена потомков идут подряд.
// Visitor вызывается с (name, isDirectory, size) не более pageSize раз.
template <typename Visitor>
bool listDirectory(lexxos::vfs::FileHandle& file, Index const& index,
    std::string_view path, std::uint32_t firstIndex, std::uint32_t pageSize,
    std::uint32_t& written, Visitor&& visitor) {
	written = 0;
	if (path != ":/") {
		if (path.size() > 2 && path.back() == '/') {
			path.remove_suffix(1);
		}

		if (!validAlias(path)) {
			return false;
		}
	}

	bool directoryExists = path == ":/";
	if (!index.present) {
		return directoryExists;
	}

	std::array<char, maximumComponentBytes + 1> previous{};
	std::uint32_t seen = 0;
	bool const valid = enumerate(file, index, [&](Entry const& entry) {
		std::string_view const alias{entry.alias.data()};
		std::size_t const start = path == ":/" ? 2 : path.size() + 1;
		if (alias.size() <= start || !alias.starts_with(path) || (path != ":/" && alias[path.size()] != '/')) {
			return true;
		}

		directoryExists = true;
		auto const tail = alias.substr(start);
		auto const slash = tail.find('/');
		auto const child = tail.substr(0, slash);
		if (child == previous.data()) {
			return true;
		}

		std::memset(previous.data(), 0, previous.size());
		std::memcpy(previous.data(), child.data(), child.size());
		if (seen++ < firstIndex || written >= pageSize) {
			return true;
		}

		visitor(child, slash != std::string_view::npos,
		    slash == std::string_view::npos ? entry.size : 0U);
		++written;
		return true;
	});
	return valid && directoryExists;
}

} // namespace lexxos::external_program::resources
