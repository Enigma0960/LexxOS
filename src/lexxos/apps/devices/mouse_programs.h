#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs {

class MouseInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

class MouseResetProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

class MouseLiveProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

class UsbHidInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::device_programs
