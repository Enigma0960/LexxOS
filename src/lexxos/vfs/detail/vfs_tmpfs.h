#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/vfs/vfs_internal_types.h>

namespace lexxos::vfs::detail {

void initializeTmpfs();

bool isTmpFilePath(std::string_view path);
std::string_view tmpFileName(std::string_view path);
TmpFile* findTmpFileByName(std::string_view name);
TmpFile* tmpFileByIndex(std::size_t index);
std::size_t tmpFileIndex(TmpFile const* target);
TmpFile* ensureTmpFile(std::string_view name);

bool statTmpFile(std::string_view path, Stat& output);
bool listTmp(DirectoryVisitor visitor, void* userData);
ReadStatus readTmpText(std::string_view path, TextWriter writer, void* userData);
RemoveStatus removeTmpFile(std::string_view path);
RenameStatus renameTmpFile(std::string_view source, std::string_view target);

} // namespace lexxos::vfs::detail
