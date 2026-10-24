#pragma once

#include <string_view>

#include <lexxos/vfs/vfs_internal_types.h>

namespace lexxos::vfs::detail {

void initializeMountTable();

BlockDeviceMount* findBlockDeviceByPath(std::string_view path);
FilesystemMount* findFilesystemByPath(std::string_view path);
std::string_view pathInsideMount(FilesystemMount const& mount, std::string_view path);

bool listBlockDevices(DirectoryVisitor visitor, void* userData);
bool listMountTargets(DirectoryVisitor visitor, void* userData);

} // namespace lexxos::vfs::detail
