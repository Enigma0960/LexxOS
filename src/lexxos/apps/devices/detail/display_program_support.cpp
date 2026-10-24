#include <lexxos/apps/devices/detail/display_program_support.h>

#include <cstddef>

#include <lexxos/apps/devices/detail/device_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs::detail {
namespace {

	[[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right) {
		if (left.size() != right.size()) {
			return false;
		}
		for (std::size_t index = 0; index < left.size(); ++index) {
			auto const l = left[index] >= 'A' && left[index] <= 'Z' ? static_cast<char>(left[index] - 'A' + 'a') : left[index];
			auto const r = right[index] >= 'A' && right[index] <= 'Z' ? static_cast<char>(right[index] - 'A' + 'a') : right[index];
			if (l != r) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] bool glyphRows(char character, std::uint8_t (&rows)[7]) {
		if (character >= 'a' && character <= 'z') {
			character = static_cast<char>(character - 'a' + 'A');
		}

		auto set = [&](std::uint8_t r0, std::uint8_t r1, std::uint8_t r2, std::uint8_t r3, std::uint8_t r4, std::uint8_t r5, std::uint8_t r6) {
			rows[0] = r0;
			rows[1] = r1;
			rows[2] = r2;
			rows[3] = r3;
			rows[4] = r4;
			rows[5] = r5;
			rows[6] = r6;
		};

		switch (character) {
			case ' ': set(0, 0, 0, 0, 0, 0, 0); return true;
			case '0': set(0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E); return true;
			case '1': set(0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E); return true;
			case '2': set(0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F); return true;
			case '3': set(0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E); return true;
			case '4': set(0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02); return true;
			case '5': set(0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E); return true;
			case '6': set(0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E); return true;
			case '7': set(0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return true;
			case '8': set(0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E); return true;
			case '9': set(0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C); return true;
			case 'A': set(0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return true;
			case 'B': set(0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E); return true;
			case 'C': set(0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E); return true;
			case 'D': set(0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E); return true;
			case 'E': set(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F); return true;
			case 'F': set(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10); return true;
			case 'G': set(0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F); return true;
			case 'H': set(0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return true;
			case 'I': set(0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E); return true;
			case 'J': set(0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C); return true;
			case 'K': set(0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11); return true;
			case 'L': set(0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F); return true;
			case 'M': set(0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11); return true;
			case 'N': set(0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return true;
			case 'O': set(0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return true;
			case 'P': set(0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10); return true;
			case 'Q': set(0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D); return true;
			case 'R': set(0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11); return true;
			case 'S': set(0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E); return true;
			case 'T': set(0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return true;
			case 'U': set(0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return true;
			case 'V': set(0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04); return true;
			case 'W': set(0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A); return true;
			case 'X': set(0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11); return true;
			case 'Y': set(0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04); return true;
			case 'Z': set(0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F); return true;
			case '-': set(0, 0, 0, 0x1F, 0, 0, 0); return true;
			case '_': set(0, 0, 0, 0, 0, 0, 0x1F); return true;
			case '.': set(0, 0, 0, 0, 0, 0x0C, 0x0C); return true;
			case ':': set(0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0); return true;
			case '/': set(0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10); return true;
			case '>': set(0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10); return true;
			case '<': set(0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01); return true;
			default : set(0x1F, 0x11, 0x05, 0x02, 0x04, 0, 0x04); return false;
		}
	}

} // namespace

constexpr std::uint32_t inputLockAttempts = 100;
constexpr std::uint32_t inputLockRetryDelayMs = 1;

std::uint16_t rgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
	return static_cast<std::uint16_t>(((red & 0xF8U) << 8U) | ((green & 0xFCU) << 3U) | (blue >> 3U));
}

bool parseRgb565Color(std::string_view text, std::uint16_t& color) {
	constexpr NamedColor colors[] = {
	    {  "black", 0x0000},
	    {  "white", 0xFFFF},
	    {    "red", 0xF800},
	    {  "green", 0x07E0},
	    {   "blue", 0x001F},
	    {   "cyan", 0x07FF},
	    {"magenta", 0xF81F},
	    { "yellow", 0xFFE0},
	    {   "gray", 0x8410},
	    {   "grey", 0x8410},
	    { "orange", 0xFC00},
	};
	for (auto const& namedColor : colors) {
		if (equalsIgnoreCase(text, namedColor.name)) {
			color = namedColor.value;
			return true;
		}
	}

	std::uint32_t value = 0;
	if (!parseUnsigned(text, value) || value > 0xFFFFU) {
		return false;
	}
	color = static_cast<std::uint16_t>(value);
	return true;
}

lexxos::devices::DisplayDevice* selectedDisplay(
    lexxos::os::AppContext& context,
    std::string_view deviceName) {
	return !deviceName.empty() ? context.devices().displayDevice(deviceName) : context.devices().primaryDisplayDevice();
}

lexxos::devices::DeviceResourceLock lockDisplay(
    lexxos::os::AppContext& context,
    lexxos::devices::DisplayDevice const& display,
    std::string_view owner) {
	return context.devices().lockDisplayDevice(display.name(), lexxos::devices::DeviceResourcePriority::Interactive, owner);
}

DisplayDiagnosticLocks lockDisplayDiagnosticResources(
    lexxos::os::AppContext& context,
    lexxos::devices::DisplayDevice const& display,
    std::string_view owner) {
	DisplayDiagnosticLocks locks{};
	auto& devices = context.devices();
	if (devices.inputDeviceCount() != 0) {
		auto const inputName = devices.inputDeviceAt(0).name;
		for (std::uint32_t attempt = 0; attempt < inputLockAttempts && !locks.input; ++attempt) {
			locks.input = devices.lockInputDevice(inputName, lexxos::devices::DeviceResourcePriority::Interactive, owner);
			if (!locks.input) {
				lexxos::kernel::sleepMs(inputLockRetryDelayMs);
			}
		}
		if (!locks.input) {
			return {};
		}
	}

	locks.display = lockDisplay(context, display, owner);
	if (!locks.display) {
		locks.input.release();
		return {};
	}
	return locks;
}

lexxos::app::ScopedConsoleMirrorMode enterGraphicsMode() {
	return lexxos::app::ScopedConsoleMirrorMode{lexxos::app::ConsoleMirrorMode::Graphics};
}

std::uint16_t* displayScratchLine() {
	static std::uint16_t line[displayScratchLinePixels]{};
	return line;
}

bool drawFilledRect(
    lexxos::devices::DisplayDevice& display,
    lexxos::devices::Rect const& rect,
    std::uint16_t color) {
	if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 || rect.x + rect.width > display.width() || rect.y + rect.height > display.height() || rect.width > displayScratchLinePixels) {
		return false;
	}

	auto* line = displayScratchLine();
	for (int index = 0; index < rect.width; ++index) {
		line[index] = color;
	}

	if (!display.beginFrame()) {
		return false;
	}
	for (int y = 0; y < rect.height; ++y) {
		if (!display.blitRgb565({.x = rect.x, .y = rect.y + y, .width = rect.width, .height = 1}, line, rect.width)) {
			return false;
		}
	}
	return display.endFrame();
}

bool drawTextLine(
    lexxos::devices::DisplayDevice& display,
    int x,
    int y,
    int scale,
    std::uint16_t foreground,
    std::uint16_t background,
    lexxos::program::ProgramInvocation const& invocation,
    int firstTextArgument,
    int& drawnWidth) {
	if (x < 0 || y < 0 || scale <= 0 || scale > 8 || firstTextArgument >= invocation.argc || y + 7 * scale > display.height() || x >= display.width()) {
		return false;
	}

	auto* line = displayScratchLine();
	auto const maxWidth = display.width() - x;
	if (maxWidth <= 0 || maxWidth > displayScratchLinePixels || !display.beginFrame()) {
		return false;
	}

	drawnWidth = 0;
	for (int sourceRow = 0; sourceRow < 7; ++sourceRow) {
		int lineWidth = 0;
		for (int argumentIndex = firstTextArgument; argumentIndex < invocation.argc && lineWidth < maxWidth; ++argumentIndex) {
			if (argumentIndex > firstTextArgument) {
				for (int sx = 0; sx < 3 * scale && lineWidth < maxWidth; ++sx) {
					line[lineWidth++] = background;
				}
			}

			std::string_view const word{invocation.argv[argumentIndex]};
			for (auto const character : word) {
				std::uint8_t rows[7]{};
				(void) glyphRows(character, rows);
				auto const row = rows[sourceRow];
				for (int column = 0; column < 5 && lineWidth < maxWidth; ++column) {
					auto const lit = (row & (1U << (4 - column))) != 0;
					for (int sx = 0; sx < scale && lineWidth < maxWidth; ++sx) {
						line[lineWidth++] = lit ? foreground : background;
					}
				}
				for (int sx = 0; sx < scale && lineWidth < maxWidth; ++sx) {
					line[lineWidth++] = background;
				}
			}
		}

		if (lineWidth <= 0) {
			line[lineWidth++] = background;
		}
		if (lineWidth > drawnWidth) {
			drawnWidth = lineWidth;
		}
		for (int sy = 0; sy < scale; ++sy) {
			if (!display.blitRgb565({.x = x, .y = y + sourceRow * scale + sy, .width = lineWidth, .height = 1}, line, lineWidth)) {
				return false;
			}
		}
	}
	return display.endFrame();
}

} // namespace lexxos::apps::device_programs::detail
