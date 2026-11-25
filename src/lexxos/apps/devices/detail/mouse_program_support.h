#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs_types.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

class ParsedArguments;

} // namespace lexxos::program

namespace lexxos::devices {

class PointingDevice;
class DisplayDevice;
struct PointingDeviceState;
struct TouchCalibration;
struct TouchContact;

} // namespace lexxos::devices

namespace lexxos::apps::device_programs::detail::mouse {

void writeSigned(lexxos::os::AppContext& context, std::int32_t value);
void writeState(lexxos::os::AppContext& context, lexxos::devices::PointingDeviceState const& state);
lexxos::devices::PointingDevice* selectMouse(lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments, std::string_view command);

} // namespace lexxos::apps::device_programs::detail::mouse
