#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::filesystem_programs {

class LinkCreateProgram final {
public:
    static lexxos::program::ProgramDescriptor descriptor();
    static int run(lexxos::os::AppContext&,
        lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};
class LinkRunProgram final {
public:
    static lexxos::program::ProgramDescriptor descriptor();
    static int run(lexxos::os::AppContext&,
        lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};
class AutostartProgram final {
public:
    static lexxos::program::ProgramDescriptor descriptor();
    static int run(lexxos::os::AppContext&,
        lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::filesystem_programs
