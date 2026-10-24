#include <lexxos/app/detail/picocalc_lcd_console_mirror_sink.h>

#include <lexxos/app/picocalc_lcd_console.h>

namespace lexxos::app::detail {

void PicoCalcLcdConsoleMirrorSink::initialize(lexxos::os::OperatingSystem& operatingSystem) {
	lexxos::app::picocalc_lcd_console::initialize(operatingSystem);
}

void PicoCalcLcdConsoleMirrorSink::write(std::string_view text) {
	lexxos::app::picocalc_lcd_console::write(text);
}

void PicoCalcLcdConsoleMirrorSink::flush() {
	lexxos::app::picocalc_lcd_console::flush();
}

void PicoCalcLcdConsoleMirrorSink::clear() {
	lexxos::app::picocalc_lcd_console::clear();
}

void PicoCalcLcdConsoleMirrorSink::refresh() {
	lexxos::app::picocalc_lcd_console::refresh();
}

void PicoCalcLcdConsoleMirrorSink::setMode(lexxos::app::ConsoleMirrorMode mode) {
	_mode = mode;
	lexxos::app::picocalc_lcd_console::setTextModeEnabled(mode == lexxos::app::ConsoleMirrorMode::Text);
}

lexxos::app::ConsoleMirrorMode PicoCalcLcdConsoleMirrorSink::mode() const {
	return _mode;
}

bool PicoCalcLcdConsoleMirrorSink::update() {
	return lexxos::app::picocalc_lcd_console::update();
}

bool PicoCalcLcdConsoleMirrorSink::isReady() const {
	return lexxos::app::picocalc_lcd_console::isReady();
}

} // namespace lexxos::app::detail
