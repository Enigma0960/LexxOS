#pragma once

#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::vfs::detail {

void initializePseudoFiles(lexxos::platform::PlatformServices const& platform);
bool isKnownDirectory(std::string_view path);
bool isKnownStaticFile(std::string_view path);
bool statPseudoNode(std::string_view path, Stat& output);
bool listPseudoDirectory(std::string_view path, DirectoryVisitor visitor, void* userData);
ReadStatus readPseudoText(std::string_view path, TextWriter writer, void* userData);

} // namespace lexxos::vfs::detail
