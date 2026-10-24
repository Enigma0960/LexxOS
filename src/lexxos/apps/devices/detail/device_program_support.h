#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::apps::device_programs::detail {

[[nodiscard]] bool parseUnsigned(std::string_view text, std::uint32_t& output);

} // namespace lexxos::apps::device_programs::detail
