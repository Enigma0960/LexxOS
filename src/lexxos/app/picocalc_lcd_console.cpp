#include <lexxos/app/picocalc_lcd_console.h>

#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/board_config.h>
#include <lexxos/feature.h>

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	#include <lexxos/app/detail/picocalc_lcd_console_renderer.h>

namespace {

lexxos::app::picocalc_lcd_console::detail::LcdConsoleRenderer& renderer() {
	static lexxos::app::picocalc_lcd_console::detail::LcdConsoleRenderer instance;
	return instance;
}

} // namespace

namespace lexxos::app::picocalc_lcd_console {

void initialize(lexxos::os::OperatingSystem& operatingSystem) {
	renderer().initialize(operatingSystem);
}

void write(std::string_view text) {
	renderer().write(text);
}

void flush() {
	renderer().flush();
}

void clear() {
	renderer().clear();
}

void refresh() {
	renderer().refresh();
}

void setStatusBarInfo(StatusBarInfo const& info) {
	renderer().setStatusBarInfo(info);
}

void refreshStatusBar() {
	renderer().refreshStatusBar();
}

void setTextModeEnabled(bool enabled) {
	renderer().setTextModeEnabled(enabled);
}

void scrollbackUp(int lines) {
	renderer().scrollbackUp(lines);
}

void scrollbackDown(int lines) {
	renderer().scrollbackDown(lines);
}

void scrollbackHome() {
	renderer().scrollbackHome();
}

void scrollbackEnd() {
	renderer().scrollbackEnd();
}

void scrollbackLeft(int columns) {
	renderer().scrollbackLeft(columns);
}

void scrollbackRight(int columns) {
	renderer().scrollbackRight(columns);
}

void scrollbackLeftEdge() {
	renderer().scrollbackLeftEdge();
}

void scrollbackRightEdge() {
	renderer().scrollbackRightEdge();
}

void scrollbackResetView() {
	renderer().scrollbackResetView();
}

bool isScrollbackActive() {
	return renderer().isScrollbackActive();
}

ConsoleInfo info() {
	return renderer().info();
}

ConsoleRenderStats renderStats() {
	return renderer().renderStats();
}

void resetRenderStats() {
	renderer().resetRenderStats();
}

bool update() {
	return renderer().update();
}

bool isReady() {
	return renderer().isReady();
}

} // namespace lexxos::app::picocalc_lcd_console

#else

namespace lexxos::app::picocalc_lcd_console {

void initialize(lexxos::os::OperatingSystem&) {
}

void write(std::string_view) {
}

void flush() {
}

void clear() {
}

void refresh() {
}

void setStatusBarInfo(StatusBarInfo const&) {
}

void refreshStatusBar() {
}

void setTextModeEnabled(bool) {
}

void scrollbackUp(int) {
}

void scrollbackDown(int) {
}

void scrollbackHome() {
}

void scrollbackEnd() {
}

void scrollbackLeft(int) {
}

void scrollbackRight(int) {
}

void scrollbackLeftEdge() {
}

void scrollbackRightEdge() {
}

void scrollbackResetView() {
}

bool isScrollbackActive() {
	return false;
}

ConsoleInfo info() {
	return {};
}

ConsoleRenderStats renderStats() {
	return {};
}

void resetRenderStats() {
}

bool update() {
	return false;
}

bool isReady() {
	return false;
}

} // namespace lexxos::app::picocalc_lcd_console

#endif
