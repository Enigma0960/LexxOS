#pragma once
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::vfs {

struct Stat;

} // namespace lexxos::vfs

namespace lexxos::apps::filesystem_programs::detail {

bool isLexxInfoPath(std::string_view path);
int inspectFileInfo(os::AppContext& context, std::string_view path, vfs::Stat const& stat);

} // namespace lexxos::apps::filesystem_programs::detail
