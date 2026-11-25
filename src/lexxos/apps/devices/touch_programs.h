#pragma once

#include <string_view>

#include <lexxos/apps/devices/touch_calibrate_program.h>
#include <lexxos/apps/devices/touch_draw_program.h>
#include <lexxos/apps/devices/touch_info_program.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs {

inline constexpr std::string_view touchCalibrationConfigDirectory = "/mnt/sd/.lexx";
inline constexpr std::string_view touchCalibrationConfigPath = "/mnt/sd/.lexx/touch.conf";

} // namespace lexxos::apps::device_programs
