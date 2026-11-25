#include <lexxos/app/scoped_console_mirror_mode.h>

#include <lexxos/app/console_mirror.h>

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
