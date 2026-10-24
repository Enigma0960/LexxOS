#include <lexxos/app/console_mirror.h>

#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/app/detail/null_console_mirror_sink.h>
#include <lexxos/board_config.h>
#include <lexxos/feature.h>

#if !defined(LEXXOS_TEST_FORCE_NULL_CONSOLE_MIRROR) && LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	#include <lexxos/app/detail/picocalc_lcd_console_mirror_sink.h>
#endif

namespace {

lexxos::app::ConsoleMirrorSink& selectedSink() {
#if !defined(LEXXOS_TEST_FORCE_NULL_CONSOLE_MIRROR) && LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	static lexxos::app::detail::PicoCalcLcdConsoleMirrorSink sink;
	return sink;
#else
	static lexxos::app::detail::NullConsoleMirrorSink sink;
	return sink;
#endif
}

} // namespace

namespace lexxos::app {

ScopedConsoleMirrorMode::ScopedConsoleMirrorMode(ConsoleMirrorMode mode)
    : _previousMode(console_mirror::mode())
    , _active(true) {
	console_mirror::setMode(mode);
}

ScopedConsoleMirrorMode::~ScopedConsoleMirrorMode() {
	restore();
}

void ScopedConsoleMirrorMode::restore() {
	if (!_active) {
		return;
	}
	console_mirror::setMode(_previousMode);
	_active = false;
}

} // namespace lexxos::app

namespace lexxos::app::console_mirror {

ConsoleMirrorSink& sink() {
	return selectedSink();
}

void initialize(lexxos::os::OperatingSystem& operatingSystem) {
	sink().initialize(operatingSystem);
}

void write(std::string_view text) {
	sink().write(text);
}

void flush() {
	sink().flush();
}

void clear() {
	sink().clear();
}

void refresh() {
	sink().refresh();
}

void setMode(ConsoleMirrorMode mode) {
	sink().setMode(mode);
}

ConsoleMirrorMode mode() {
	return sink().mode();
}

bool update() {
	return sink().update();
}

bool isReady() {
	return sink().isReady();
}

} // namespace lexxos::app::console_mirror
