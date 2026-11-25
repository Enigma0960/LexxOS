#pragma once
#include <cstddef>
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::filesystem_programs::detail {

inline constexpr std::size_t lexxInfoWorkspaceBytes = 4096;
int inspectLexxInfo(os::AppContext& context, std::string_view path);

} // namespace lexxos::apps::filesystem_programs::detail
