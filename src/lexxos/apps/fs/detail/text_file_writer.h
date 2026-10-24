#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

struct ParsedArguments;
struct ProgramInvocation;

} // namespace lexxos::program

namespace lexxos::apps::filesystem_programs::detail {

inline constexpr std::uint32_t textFileWriterStreamBufferBytes = 96;
inline constexpr std::uint32_t textFileWriterWorkspaceBytes =
    static_cast<std::uint32_t>(lexxos::vfs::maxPathLength + textFileWriterStreamBufferBytes + 1U);

int runTextFileWriter(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::vfs::OpenMode mode,
    std::string_view commandName);

} // namespace lexxos::apps::filesystem_programs::detail
