#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/apps/devices/detail/device_program_support.h>
#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {
class InputDevice;
struct InputEvent;
} // namespace lexxos::devices

namespace lexxos::os {
class AppContext;
}

namespace lexxos::apps::device_programs::detail {

[[nodiscard]] std::uint32_t pressedKeyMask(lexxos::devices::InputDevice& input);
[[nodiscard]] lexxos::devices::DeviceResourceLock lockInputDeviceForInteractiveProgram(
    lexxos::os::AppContext& context,
    lexxos::devices::InputDevice& input,
    std::string_view owner);

void writeBoolField(lexxos::os::AppContext& context, std::string_view name, bool value);
void writePressedKeys(lexxos::os::AppContext& context, std::uint32_t mask);
void writeRawKeyboardState(lexxos::os::AppContext& context, lexxos::devices::InputDevice& input);
void writeInputEventDetails(lexxos::os::AppContext& context, lexxos::devices::InputEvent const& event);
std::uint32_t drainInputEvents(lexxos::os::AppContext& context, lexxos::devices::InputDevice& input);

} // namespace lexxos::apps::device_programs::detail
