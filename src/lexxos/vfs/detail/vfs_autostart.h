#pragma once

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::vfs::detail {

inline constexpr std::string_view autostartVirtualPath = "/sys/autostart";
inline constexpr std::string_view autostartStoragePath = "/mnt/sd/.lexx/autostart";

inline bool isAutostartPath(std::string_view path) {
    return path == autostartVirtualPath
        || (path.size() > autostartVirtualPath.size()
            && path.substr(0, autostartVirtualPath.size()) == autostartVirtualPath
            && path[autostartVirtualPath.size()] == '/');
}

// FAT has no symbolic links. The /sys tree is a persistent, VFS-level alias of
// .lexx/autostart on the boot SD. Reject traversal in raw (unnormalized) paths.
inline bool autostartStoragePathFor(std::string_view path,
    std::array<char, maxPathLength>& output) {
    if (!isAutostartPath(path)) {
        return false;
    }
    auto suffix = path.substr(autostartVirtualPath.size());
    if (suffix.find("//") != std::string_view::npos) {
        return false;
    }
    for (std::size_t start = 0; start < suffix.size();) {
        if (suffix[start] != '/') return false;
        const auto next = suffix.find('/', start + 1);
        const auto part = suffix.substr(start + 1,
            next == std::string_view::npos ? suffix.size() - start - 1 : next - start - 1);
        if (part.empty() || part == "." || part == "..") return false;
        if (next == std::string_view::npos) break;
        start = next;
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
