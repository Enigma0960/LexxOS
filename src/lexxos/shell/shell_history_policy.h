#pragma once

#include <string_view>

#include <lexxos/program/arguments.h>

namespace lexxos::shell::detail {

using SensitiveCommandLookup = bool (*)(std::string_view name, void const* context);

[[nodiscard]] bool hasSensitiveArguments(lexxos::program::ProgramArgumentList arguments);
[[nodiscard]] bool historyLineAllowed(
    std::string_view line,
    SensitiveCommandLookup lookup,
    void const* context);

} // namespace lexxos::shell::detail
