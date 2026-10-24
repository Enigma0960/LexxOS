#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/console_mirror.h>

namespace lexxos::os {
class OperatingSystem;
}

namespace lexxos::app::picocalc_lcd_console {

struct ConsoleRenderStats {
	std::uint32_t flushCalls{0};
	std::uint32_t refreshCalls{0};
	std::uint32_t beginFrameCalls{0};
	std::uint32_t beginFrameFailures{0};
	std::uint32_t displayLockFailures{0};
	std::uint32_t charactersProcessed{0};
	std::uint32_t drawRowCalls{0};
	std::uint32_t drawCellCalls{0};
	std::uint32_t dirtyRowsDrawn{0};
	std::uint32_t scrollRowsDrawn{0};
	std::uint32_t statusBarDraws{0};
	std::uint32_t hardwareScrollPreFlushCalls{0};
	std::uint32_t hardwareScrollAdvanceCalls{0};
	std::uint32_t hardwareScrollPixels{0};
	std::uint32_t hardwareScrollConfigureCalls{0};
	std::uint32_t hardwareScrollConfigureFailures{0};
	std::uint32_t hardwareScrollDirtyShiftCalls{0};
	std::uint32_t clearCalls{0};
	std::uint32_t clearTextAreaCalls{0};
	std::uint32_t clearTextAreaFailures{0};
	std::uint32_t clearTextAreaPixels{0};
};

struct ConsoleInfo {
	bool ready{false};
	bool textMode{false};
	int columns{0};
	int rows{0};
	int logicalColumns{0};
	int viewLeftColumn{0};
	int maxViewLeftColumn{0};
	int cursorX{0};
	int cursorY{0};
	std::size_t queueCount{0};
	std::size_t scrollbackCapacity{0};
	std::size_t scrollbackCount{0};
	std::uint32_t scrollbackDroppedRows{0};
	int viewTopLine{0};
	bool viewFollowTail{true};
	bool hardwareScrollEnabled{false};
	int scrollAreaPixels{0};
	int scrollStartPixels{0};
	bool drawCursor{false};
	bool cursorVisible{false};
	int statusBarPixels{0};
	bool statusBarVisible{false};
	bool statusBarBatteryValid{false};
	std::uint8_t statusBarBatteryPercent{0};
	bool statusBarBatteryCharging{false};
	bool statusBarTimeValid{false};
	std::uint8_t statusBarHour{0};
	std::uint8_t statusBarMinute{0};
	std::uint8_t statusBarSecond{0};
	bool statusBarWifiValid{false};
	bool statusBarWifiConnected{false};
	std::uint8_t statusBarWifiSignalLevel{0};
	std::uint32_t statusBarUpdatedMs{0};
};

struct StatusBarInfo {
	bool visible{true};
	bool batteryValid{false};
	std::uint8_t batteryPercent{0};
	bool batteryCharging{false};
	bool timeValid{false};
	std::uint8_t hour{0};
	std::uint8_t minute{0};
	std::uint8_t second{0};
	bool wifiValid{false};
	bool wifiConnected{false};
	std::uint8_t wifiSignalLevel{0};
	std::uint32_t updatedMs{0};
};

void initialize(lexxos::os::OperatingSystem& operatingSystem);
void write(std::string_view text);
void flush();
void clear();
void refresh();
void setStatusBarInfo(StatusBarInfo const& info);
void refreshStatusBar();
void setMode(lexxos::app::ConsoleMirrorMode mode);
[[nodiscard]] lexxos::app::ConsoleMirrorMode mode();

void setTextModeEnabled(bool enabled);

void scrollbackUp(int lines);
void scrollbackDown(int lines);
void scrollbackHome();
void scrollbackEnd();
void scrollbackLeft(int columns);
void scrollbackRight(int columns);
void scrollbackLeftEdge();
void scrollbackRightEdge();
void scrollbackResetView();
[[nodiscard]] bool isScrollbackActive();
[[nodiscard]] ConsoleInfo info();
[[nodiscard]] ConsoleRenderStats renderStats();
void resetRenderStats();

bool update();

[[nodiscard]] bool isReady();

} // namespace lexxos::app::picocalc_lcd_console
