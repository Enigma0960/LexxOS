#include <lexxos/devices/display/detail/display_frame_guard.h>

#include <lexxos/devices/display/double_buffered_display.h>

namespace lexxos::devices::detail {

DisplayFrameGuard::DisplayFrameGuard(DoubleBufferedDisplay const& display)
    : _sync(display._synchronization) {
	if (_sync.enter && _sync.leave) {
		_state = _sync.enter(_sync.context);
	}
}

DisplayFrameGuard::~DisplayFrameGuard() {
	if (_sync.enter && _sync.leave) {
		_sync.leave(_sync.context, _state);
	}
}

} // namespace lexxos::devices::detail
