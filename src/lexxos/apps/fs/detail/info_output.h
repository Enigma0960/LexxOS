#pragma once
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::filesystem_programs::detail {

void writeInfoLabel(os::AppContext& context, std::string_view key, std::string_view value);

} // namespace lexxos::apps::filesystem_programs::detail
