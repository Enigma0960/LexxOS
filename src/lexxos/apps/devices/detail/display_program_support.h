#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/app/console_mirror.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::os {
class AppContext;
}

namespace lexxos::program {
struct ProgramInvocation;
}

namespace lexxos::apps::device_programs::detail {

#ifndef LEXXOS_DISPLAY_SCRATCH_LINE_PIXELS
	#define LEXXOS_DISPLAY_SCRATCH_LINE_PIXELS 320
#endif

constexpr int displayScratchLinePixels = LEXXOS_DISPLAY_SCRATCH_LINE_PIXELS;

struct NamedColor {
	std::string_view name;
	std::uint16_t value;
};

struct DisplayDiagnosticLocks {
	lexxos::devices::DeviceResourceLock input;
	lexxos::devices::DeviceResourceLock display;

	[[nodiscard]] explicit operator bool() const {
		return display.valid();
	}
};

[[nodiscard]] std::uint16_t rgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue);
[[nodiscard]] bool parseRgb565Color(std::string_view text, std::uint16_t& color);
[[nodiscard]] lexxos::devices::DisplayDevice* selectedDisplay(
    lexxos::os::AppContext& context,
    std::string_view deviceName);
[[nodiscard]] lexxos::devices::DeviceResourceLock lockDisplay(
    lexxos::os::AppContext& context,
    lexxos::devices::DisplayDevice const& display,
    std::string_view owner);
[[nodiscard]] DisplayDiagnosticLocks lockDisplayDiagnosticResources(
    lexxos::os::AppContext& context,
    lexxos::devices::DisplayDevice const& display,
    std::string_view owner);
[[nodiscard]] lexxos::app::ScopedConsoleMirrorMode enterGraphicsMode();
[[nodiscard]] std::uint16_t* displayScratchLine();
[[nodiscard]] bool drawFilledRect(
    lexxos::devices::DisplayDevice& display,
    lexxos::devices::Rect const& rect,
    std::uint16_t color);
[[nodiscard]] bool drawTextLine(
    lexxos::devices::DisplayDevice& display,
    int x,
    int y,
    int scale,
    std::uint16_t foreground,
    std::uint16_t background,
    lexxos::program::ProgramInvocation const& invocation,
    int firstTextArgument,
    int& drawnWidth);

} // namespace lexxos::apps::device_programs::detail
