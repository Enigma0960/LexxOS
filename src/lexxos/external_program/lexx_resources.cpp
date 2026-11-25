#include <lexxos/external_program/lexx_resources.h>

#include <algorithm>

namespace lexxos::external_program::resources {

bool readExact(lexxos::vfs::FileHandle& file, void* output, std::size_t bytes) {
	auto* destination = static_cast<std::uint8_t*>(output);
	while (bytes != 0) {
		std::size_t received = 0;
		if (file.read(destination, bytes, received) != lexxos::vfs::ReadStatus::Ok || received == 0) {
			return false;
		}

		destination += received;
		bytes -= received;
	}

	return true;
}

bool validAlias(std::string_view alias) {
	if (alias.size() < 3 || alias.size() > maximumAliasBytes || alias.substr(0, 2) != ":/") {
		return false;
	}

	std::size_t segmentStart = 2;
	for (std::size_t i = 2; i <= alias.size(); ++i) {
		if (i != alias.size() && alias[i] != '/') {
			if (alias[i] == '\\' || alias[i] == ':' || alias[i] == '\0') {
				return false;
			}

			continue;
		}

		auto const part = alias.substr(segmentStart, i - segmentStart);
		if (part.empty() || part.size() > maximumComponentBytes || part == "." || part == "..") {
			return false;
		}

		segmentStart = i + 1;
	}

	return true;
}

bool readEntry(lexxos::vfs::FileHandle& file, Index const& index,
    std::uint64_t& cursor, Entry& entry) {
	if (cursor > index.tableEnd || index.tableEnd - cursor < 10 || !file.seek(cursor)) {
		return false;
	}

	std::uint8_t header[10]{};
	if (!readExact(file, header, sizeof(header))) {
		return false;
	}

	entry.offset = read32(header);
	entry.size = read32(header + 4);
	auto const aliasBytes = read16(header + 8);
	cursor += sizeof(header);
	if (aliasBytes == 0 || aliasBytes > maximumAliasBytes || aliasBytes > index.tableEnd - cursor || entry.offset < index.imageEnd || entry.offset > index.tableStart || entry.size > index.tableStart - entry.offset) {
		return false;
	}

	entry.alias.fill('\0');
	if (!readExact(file, entry.alias.data(), aliasBytes)) {
		return false;
	}

	cursor += aliasBytes;
	return validAlias(std::string_view{entry.alias.data(), aliasBytes});
}

InspectStatus inspect(lexxos::vfs::FileHandle& file, std::uint64_t imageEnd, Index& result) {
	// Поиск манифеста и проверка границ таблицы ресурсов.
	result = {};
	result.imageEnd = imageEnd;
	if (file.size() < imageEnd + 12) {
		return InspectStatus::None;
	}

	std::uint8_t manifestFooter[12]{};
	if (!file.seek(file.size() - sizeof(manifestFooter))
	    || !readExact(file, manifestFooter, sizeof(manifestFooter))) {
		return InspectStatus::Invalid;
	}

	if (std::memcmp(manifestFooter, "LXMF", 4) != 0) {
		return InspectStatus::None; // Старый пакет без манифеста.
	}

	auto const manifestBytes = read32(manifestFooter + 4);
	if (manifestBytes == 0 || manifestBytes > 4096 || file.size() - imageEnd - 12 < manifestBytes) {
		return InspectStatus::Invalid;
	}

	auto const manifestStart = file.size() - 12 - manifestBytes;
	if (manifestStart - imageEnd < footerBytes) {
		return InspectStatus::None; // Манифест есть, ресурсов нет.
	}

	std::uint8_t footer[footerBytes]{};
	auto const footerStart = manifestStart - footerBytes;
	if (!file.seek(footerStart) || !readExact(file, footer, sizeof(footer))) {
		return InspectStatus::Invalid;
	}

	bool const indexed = std::memcmp(footer, "LXR2", 4) == 0;
	if (!indexed && std::memcmp(footer, "LXRS", 4) != 0) {
		return InspectStatus::None;
	}

	auto const count = read32(footer + 4);
	auto const tableBytes = read32(footer + 8);
	std::uint64_t const offsetsBytes = indexed ? static_cast<std::uint64_t>(count) * 4U : 0U;
	if (count == 0 || count > maximumEntries || tableBytes > maximumTableBytes || tableBytes < static_cast<std::uint64_t>(count) * 11U + offsetsBytes || tableBytes > footerStart - imageEnd) {
		return InspectStatus::Invalid;
	}

	result = {.tableStart = footerStart - tableBytes,
	    .tableEnd = footerStart - offsetsBytes,
	    .offsetsStart = footerStart - offsetsBytes,
	    .imageEnd = imageEnd,
	    .count = count,
	    .present = true,
	    .indexed = indexed};

	// Проверка CRC только для индекса; данные ресурсов не копируются в RAM.
	std::uint32_t crc = 0xFFFFFFFFU;
	std::array<std::uint8_t, 128> buffer{};
	if (!file.seek(result.tableStart)) {
		return InspectStatus::Invalid;
	}

	std::uint32_t remaining = tableBytes;
	while (remaining != 0) {
		auto const n = static_cast<std::size_t>(std::min<std::uint32_t>(remaining, buffer.size()));
		if (!readExact(file, buffer.data(), n)) {
			return InspectStatus::Invalid;
		}

		for (std::size_t i = 0; i < n; ++i) {
			crc ^= buffer[i];
			for (unsigned bit = 0; bit < 8; ++bit) {
				crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
			}
		}

		remaining -= static_cast<std::uint32_t>(n);
	}

	if ((crc ^ 0xFFFFFFFFU) != read32(footer + 12)) {
		return InspectStatus::Invalid;
	}

	if (indexed) {
		// После CRC проверяем монотонность таблицы смещений последовательным чтением.
		// Это исключает тысячи обращений к отдельным записям при запуске.
		// readEntry() проверяет каждую запрошенную запись перед её возвратом.
		if (!file.seek(result.offsetsStart)) {
			return InspectStatus::Invalid;
		}

		std::uint32_t previous = 0U;
		std::uint32_t scanned = 0U;
		std::uint32_t const recordBytes = static_cast<std::uint32_t>(result.tableEnd - result.tableStart);
		while (scanned < count) {
			std::uint32_t const batch = std::min<std::uint32_t>(count - scanned, buffer.size() / 4U);
			if (!readExact(file, buffer.data(), batch * 4U)) {
				return InspectStatus::Invalid;
			}

			for (std::uint32_t i = 0; i < batch; ++i) {
				std::uint32_t const offset = read32(buffer.data() + i * 4U);
				if ((scanned + i == 0U ? offset != 0U : offset <= previous)
				    || offset > recordBytes
				    || recordBytes - offset < 11U) {
					return InspectStatus::Invalid;
				}

				previous = offset;
			}

			scanned += batch;
		}

		return InspectStatus::Valid;
	}

	// Проверка порядка псевдонимов и конфликтов файлов с каталогами в LXRS.
	std::uint64_t cursor = result.tableStart;
	Entry entry{};
	std::array<char, maximumAliasBytes + 1> previous{};
	for (std::uint32_t n = 0; n < result.count; ++n) {
		if (!readEntry(file, result, cursor, entry)) {
			return InspectStatus::Invalid;
		}

		if (n != 0) {
			std::string_view const earlier{previous.data()};
			std::string_view const current{entry.alias.data()};
			if (earlier >= current || (current.starts_with(earlier) && current.size() > earlier.size() && current[earlier.size()] == '/')) {
				return InspectStatus::Invalid; // Дубликат, нарушение порядка или конфликт файла с каталогом.
			}
		}

		previous = entry.alias;
	}

	return cursor == result.tableEnd ? InspectStatus::Valid : InspectStatus::Invalid;
}

bool find(lexxos::vfs::FileHandle& file, Index const& index,
    std::string_view alias, Entry& result) {
	if (!index.present || !validAlias(alias)) {
		return false;
	}

	if (index.indexed) {
		std::uint32_t first = 0;
		std::uint32_t last = index.count;
		while (first < last) {
			std::uint32_t const middle = first + (last - first) / 2U;
			std::uint8_t encoded[4]{};
			if (!file.seek(index.offsetsStart + static_cast<std::uint64_t>(middle) * 4U)
			    || !readExact(file, encoded, sizeof(encoded))) {
				return false;
			}

			std::uint64_t cursor = index.tableStart + read32(encoded);
			Entry entry{};
			if (!readEntry(file, index, cursor, entry)) {
				return false;
			}

			int const order = alias.compare(entry.alias.data());
			if (order == 0) {
				result = entry;
				return true;
			}

			if (order < 0) {
				last = middle;
			} else {
				first = middle + 1U;
			}
		}

		return false;
	}

	std::uint64_t cursor = index.tableStart;
	for (std::uint32_t n = 0; n < index.count; ++n) {
		Entry entry{};
		if (!readEntry(file, index, cursor, entry)) {
			return false;
		}

		if (alias == entry.alias.data()) {
			result = entry;
			return true;
		}
	}

	return false;
}

PathKind lookup(lexxos::vfs::FileHandle& file, Index const& index,
    std::string_view path, Entry& output) {
	if (path == ":/") {
		return PathKind::Directory;
	}

	bool const trailingSlash = path.size() > 2 && path.back() == '/';
	if (trailingSlash) {
		path.remove_suffix(1);
	}

	if (!validAlias(path) || !index.present) {
		return PathKind::Missing;
	}

	if (!trailingSlash && find(file, index, path, output)) {
		return PathKind::File;
	}

	PathKind kind = PathKind::Missing;
	auto const valid = enumerate(file, index, [&](Entry const& entry) {
		std::string_view const name{entry.alias.data()};
		if (!trailingSlash && name == path) {
			output = entry;
			kind = PathKind::File;
		} else if (name.size() > path.size() && name.starts_with(path) && name[path.size()] == '/') {
			kind = PathKind::Directory;
		}

		return true;
	});
	return valid ? kind : PathKind::Missing;
}

} // namespace lexxos::external_program::resources
