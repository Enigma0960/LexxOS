#pragma once
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::filesystem_programs::detail {

int inspectShortcutInfo(os::AppContext& context, std::string_view path);

} // namespace lexxos::apps::filesystem_programs::detail
