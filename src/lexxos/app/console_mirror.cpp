#include <lexxos/app/console_mirror.h>

#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/app/detail/null_console_mirror_sink.h>
#include <lexxos/board_config.h>
#include <lexxos/feature.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	#include <lexxos/platform/platform.h>
#endif

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

} // namespace lexxos::app

namespace lexxos::app::console_mirror {

ConsoleMirrorSink& sink() {
	return selectedSink();
}

void initialize(lexxos::os::OperatingSystem& operatingSystem) {
#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[lcd-mirror] selecting sink\n");
#endif

	auto& target = sink();

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[lcd-mirror] dispatch initialize\n");
#endif

	target.initialize(operatingSystem);

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[lcd-mirror] initialize returned\n");
#endif
}

void write(std::string_view text) {
	if (!isSuspended()) {
		sink().write(text);
	}
}

void flush() {
	if (!isSuspended()) {
		sink().flush();
	}
}

void clear() {
	if (!isSuspended()) {
		sink().clear();
	}
}

void refresh() {
	if (!isSuspended()) {
		sink().refresh();
	}
}

void setMode(ConsoleMirrorMode mode) {
	sink().setMode(mode);
}

ConsoleMirrorMode mode() {
	return sink().mode();
}

bool update() {
	return !isSuspended() && sink().update();
}

bool isReady() {
	return sink().isReady();
}

} // namespace lexxos::app::console_mirror
