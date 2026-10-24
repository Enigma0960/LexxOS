#pragma once

#include <string_view>

#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs {

inline constexpr std::string_view touchCalibrationConfigDirectory = "/mnt/sd/.lexx";
inline constexpr std::string_view touchCalibrationConfigPath = "/mnt/sd/.lexx/touch.conf";

class TouchInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

class TouchCalibrateProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

class TouchDrawProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::device_programs
