#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::app::picocalc_lcd_console::detail {

#ifndef LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	#if defined(PICO_RP2350) && PICO_RP2350
		#define LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH 1
	#else
		#define LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH 0
	#endif
#endif

constexpr std::string_view consoleDisplayName{"ili9488-spi"};
constexpr std::string_view consoleOwner{"picocalc-console"};
// RP2040 keeps both LCD orientations (up to 80 columns / 60 rows)
// while bounding terminal history to leave SRAM for Wi-Fi and the RTOS.
#if defined(PICO_RP2040) && PICO_RP2040
constexpr int maxColumns = 80;
constexpr int maxVisibleColumns = 80;
constexpr int maxRows = 60;
constexpr std::size_t queueCapacity = 2048;
constexpr int scrollbackRows = 32;
#else
constexpr int maxColumns = 256;
constexpr int maxVisibleColumns = 160;
constexpr int maxRows = 64;
constexpr std::size_t queueCapacity = 8192;
constexpr int scrollbackRows = 96;
#endif
constexpr int maxScrollbackColumns = maxVisibleColumns;
constexpr int glyphWidth = 5;
constexpr int glyphHeight = 7;
constexpr int cellWidth = 6;
constexpr int cellHeight = 8;
constexpr char blankCharacter = ' ';
constexpr std::uint16_t foregroundColor = 0xFFFF;
constexpr std::uint16_t backgroundColor = 0x0000;
constexpr std::uint16_t ansiBlackColor = 0x0000;
constexpr std::uint16_t ansiRedColor = 0xF800;
constexpr std::uint16_t ansiGreenColor = 0x07E0;
constexpr std::uint16_t ansiYellowColor = 0xFFE0;
constexpr std::uint16_t ansiBlueColor = 0x001F;
constexpr std::uint16_t ansiMagentaColor = 0xF81F;
constexpr std::uint16_t ansiCyanColor = 0x07FF;
constexpr std::uint16_t ansiWhiteColor = 0xC618;
constexpr std::uint16_t ansiBrightBlackColor = 0x7BEF;
constexpr std::uint16_t ansiBrightRedColor = 0xFBEF;
constexpr std::uint16_t ansiBrightGreenColor = 0x87F0;
constexpr std::uint16_t ansiBrightYellowColor = 0xFFF0;
constexpr std::uint16_t ansiBrightBlueColor = 0x841F;
constexpr std::uint16_t ansiBrightMagentaColor = 0xF81F;
constexpr std::uint16_t ansiBrightCyanColor = 0x87FF;
constexpr std::uint16_t ansiBrightWhiteColor = 0xFFFF;
constexpr std::uint8_t paletteBlack = 0;
constexpr std::uint8_t paletteRed = 1;
constexpr std::uint8_t paletteGreen = 2;
constexpr std::uint8_t paletteYellow = 3;
constexpr std::uint8_t paletteBlue = 4;
constexpr std::uint8_t paletteMagenta = 5;
constexpr std::uint8_t paletteCyan = 6;
constexpr std::uint8_t paletteWhite = 7;
constexpr std::uint8_t paletteBrightBlack = 8;
constexpr std::uint8_t paletteBrightRed = 9;
constexpr std::uint8_t paletteBrightGreen = 10;
constexpr std::uint8_t paletteBrightYellow = 11;
constexpr std::uint8_t paletteBrightBlue = 12;
constexpr std::uint8_t paletteBrightMagenta = 13;
constexpr std::uint8_t paletteBrightCyan = 14;
constexpr std::uint8_t paletteBrightWhite = 15;
constexpr std::uint8_t defaultForegroundPalette = paletteBrightWhite;
constexpr std::uint8_t defaultBackgroundPalette = paletteBlack;
constexpr std::uint8_t defaultCellAttribute =
    static_cast<std::uint8_t>((defaultBackgroundPalette << 4U) | defaultForegroundPalette);
constexpr int statusBarHeightPixels = 24;
constexpr int statusBarPaddingLeft = 4;
constexpr int statusBarPaddingRight = 6;
constexpr int statusBarComponentGap = 4;
constexpr int statusBarTextY = 8;
constexpr int statusBarBatteryWidth = 26;
constexpr int statusBarBatteryHeight = 10;
constexpr int statusBarBatteryTerminalWidth = 3;
constexpr int statusBarBatteryIconWidth = statusBarBatteryWidth + statusBarBatteryTerminalWidth;
constexpr int statusBarBatteryY = 7;
constexpr int statusBarWifiIconWidth = 18;
constexpr int statusBarWifiIconHeight = 15;
constexpr int statusBarWifiIconY = 3;
constexpr int maxStatusBarPixelWidth = maxVisibleColumns * cellWidth;
constexpr std::uint16_t statusBackgroundColor = 0x0000;
constexpr std::uint16_t statusSeparatorColor = 0x39E7;
constexpr std::uint16_t statusTextColor = 0xC618;
constexpr std::uint16_t statusAccentColor = 0x07E0;
constexpr std::uint16_t statusLowBatteryColor = 0xF800;
constexpr std::uint16_t statusMediumBatteryColor = 0xFFE0;
constexpr std::uint8_t lowBatteryPercent = 20;
constexpr std::uint8_t mediumBatteryPercent = 50;
constexpr int maxDirtyCellsPerRefresh = 64;
#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
constexpr int maxCharactersPerFlush = 1024;
constexpr int maxDirtyRowsPerFlush = 12;
constexpr int maxScrollRowsPerFlush = 16;
#else
constexpr int maxCharactersPerFlush = 512;
constexpr int maxDirtyRowsPerFlush = 8;
constexpr int maxScrollRowsPerFlush = 12;
#endif
constexpr int invalidRow = -1;
constexpr int fullRowPixelCount = maxVisibleColumns * cellWidth * cellHeight;
constexpr int rowPixelCount = maxVisibleColumns * cellWidth;
constexpr int maxCsiParameters = 4;
constexpr int cursorFollowMarginColumns = 8;

} // namespace lexxos::app::picocalc_lcd_console::detail
