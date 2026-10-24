#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

#include <lexxos/app/detail/picocalc_lcd_console_character_queue.h>
#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_hardware_scroll.h>
#include <lexxos/app/detail/picocalc_lcd_console_status_bar_state.h>
#include <lexxos/app/detail/picocalc_lcd_console_terminal_state.h>
#include <lexxos/app/detail/picocalc_lcd_console_text_renderer.h>
#include <lexxos/app/detail/picocalc_status_bar_renderer.h>
#include <lexxos/app/picocalc_lcd_console.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/os/operating_system.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleRenderer {
public:
	void initialize(lexxos::os::OperatingSystem& operatingSystem);

	void write(std::string_view text);

	void setStatusBarInfo(lexxos::app::picocalc_lcd_console::StatusBarInfo const& info);

	void refreshStatusBar();

	void flush();

	bool update();

	void setTextModeEnabled(bool enabled);

	void clear();

	void refresh();

	[[nodiscard]] bool isReady() const;

	void scrollbackUp(int lines);

	void scrollbackDown(int lines);

	void scrollbackHome();

	void scrollbackEnd();

	void scrollbackLeft(int columns);

	void scrollbackRight(int columns);

	void scrollbackLeftEdge();

	void scrollbackRightEdge();

	void scrollbackResetView();

	[[nodiscard]] bool isScrollbackActive() const;

	[[nodiscard]] lexxos::app::picocalc_lcd_console::ConsoleInfo info();

	[[nodiscard]] lexxos::app::picocalc_lcd_console::ConsoleRenderStats renderStats();

	void resetRenderStats();

private:
	[[nodiscard]] lexxos::devices::DeviceResourceLock _lockDisplay() const;

	[[nodiscard]] LcdConsoleTerminalRenderHooks _terminalHooks();

	static void _terminalMarkDirtyRow(void* context, int row);

	static void _terminalClearDirtyRows(void* context);

	static void _terminalSetScrollRedrawRow(void* context, int row);

	static bool _terminalHardwareScrollEnabled(void* context);

	static void _terminalFlushDirtyRowsBeforeHardwareScroll(void* context, int rows);

	static bool _terminalAdvanceHardwareScroll(void* context, int pixels);

	static void _terminalShiftDirtyRowsAfterHardwareScroll(void* context, int rows);

	void _applyViewChange(LcdConsoleScrollbackViewChange const& change);

	bool _ensureHardwareScrollConfigured();

	void _lockState();

	void _unlockState();

	void _markDirtyRow(int row);

	void _markAllRowsDirty();

	void _clearDirtyRows();

	void _clearDirtyRowsUnlocked();

	bool _flushPendingScroll(int maxRows);

	bool _drawDirtyRows(int maxRows);

	void _shiftDirtyRowsAfterHardwareScroll(int rows);

	void _drawRow(int row);

	void _drawCell(int column, int row);

	[[nodiscard]] LcdConsoleTextRenderFrame _textRenderFrame();
	[[nodiscard]] int _visibleRowsNeedingClear() const;
	[[nodiscard]] std::uint32_t _textAreaPixelCount() const;
	[[nodiscard]] std::uint32_t _textRowsPixelCount(int rows) const;
	[[nodiscard]] bool _clearTextRowsBackground(int rows);
	[[nodiscard]] bool _clearTextAreaBackground();

	void _drawStatusBar();

	lexxos::app::picocalc_lcd_console::ConsoleRenderStats _stats{};
	lexxos::os::OperatingSystem* _operatingSystem{nullptr};
	lexxos::devices::DisplayDevice* _display{nullptr};
	LcdConsoleLayout _layout{};
	LcdConsoleTerminalState _terminal{};
	bool _drawCursor{true};
	bool _ready{false};
	bool _textMode{true};
	LcdConsoleCharacterQueue _queue{};
#if !defined(LEXXOS_KERNEL_FREERTOS)
	std::atomic_flag _stateLock = ATOMIC_FLAG_INIT;
#endif
	std::array<bool, maxRows> _dirtyRows{};
	int _dirtyScanRow{0};
	int _scrollRedrawRow{invalidRow};
	LcdConsoleHardwareScroll _hardwareScroll{};
	LcdConsoleStatusBarState _statusBar{};
	PicoCalcStatusBarRenderer _statusRenderer{};
	LcdConsoleTextRenderer _textRenderer{};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
