#pragma once

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::vfs::detail {

inline constexpr std::string_view autostartVirtualPath = "/sys/autostart";
inline constexpr std::string_view autostartStoragePath = "/mnt/sd/.lexx/autostart";

inline bool isAutostartPath(std::string_view path) {
	return path == autostartVirtualPath || (path.size() > autostartVirtualPath.size() && path.substr(0, autostartVirtualPath.size()) == autostartVirtualPath && path[autostartVirtualPath.size()] == '/');
}

inline bool autostartStoragePathFor(std::string_view path,
    std::array<char, maxPathLength>& output) {
	if (!isAutostartPath(path)) {
		return false;
	}

	auto const suffix = path.substr(autostartVirtualPath.size());
	if (suffix.find("//") != std::string_view::npos) {
		return false;
	}

	for (std::size_t i = 0; i < suffix.size();) {
		if (suffix[i] != '/') {
			return false;
		}

		auto const next = suffix.find('/', i + 1);
		auto const part = suffix.substr(i + 1, next == std::string_view::npos ? suffix.size() - i - 1 : next - i - 1);
		if (part.empty() || part == "." || part == "..") {
			return false;
		}

		if (next == std::string_view::npos) {
			break;
		}

		i = next;
	}

	if (autostartStoragePath.size() + suffix.size() >= output.size()) {
		return false;
	}

	std::memcpy(output.data(), autostartStoragePath.data(), autostartStoragePath.size());
	std::memcpy(output.data() + autostartStoragePath.size(), suffix.data(), suffix.size());
	output[autostartStoragePath.size() + suffix.size()] = '\0';
	return true;
}

inline void ensureAutostartStorage(Vfs& vfs) {
	(void) vfs.createDirectory("/mnt/sd/.lexx");
	(void) vfs.createDirectory(autostartStoragePath);
}

} // namespace lexxos::vfs::detail
