#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::vfs::detail {

inline bool equals(std::string_view left, std::string_view right) {
	return left.size() == right.size() && std::memcmp(left.data(), right.data(), left.size()) == 0;
}

inline bool startsWith(std::string_view value, std::string_view prefix) {
	return value.size() >= prefix.size() && std::memcmp(value.data(), prefix.data(), prefix.size()) == 0;
}

inline bool copyText(std::string_view source, char* output, std::size_t outputCapacity) {
	if (outputCapacity == 0 || source.size() + 1 > outputCapacity) {
		return false;
	}
	std::memcpy(output, source.data(), source.size());
	output[source.size()] = '\0';
	return true;
}

inline bool visitEntry(
    DirectoryVisitor visitor,
    void* userData,
    std::string_view name,
    NodeType type,
    std::uint64_t size = 0) {
	DirectoryEntry const entry{.name = name, .type = type, .sizeBytes = size};
	return visitor(entry, userData);
}

} // namespace lexxos::vfs::detail
