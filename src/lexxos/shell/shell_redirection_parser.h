#pragma once

#include <array>
#include <string_view>

#include <lexxos/shell/shell.h>
#include <lexxos/shell/shell_redirection_types.h>

namespace lexxos::shell::detail {

[[nodiscard]] RedirectionParseResult applyRedirections(
    std::array<char const*, Shell::maxArguments> const& argv,
    int argc,
    std::array<char const*, Shell::maxArguments>& commandArgv,
    int& commandArgc,
    ShellRedirection& redirection,
    std::string_view workingDirectory);

} // namespace lexxos::shell::detail
