#include <lexxos/app/detail/null_console_mirror_sink.h>

namespace lexxos::app::detail {

void NullConsoleMirrorSink::initialize(lexxos::os::OperatingSystem&) {
}

void NullConsoleMirrorSink::write(std::string_view) {
}

void NullConsoleMirrorSink::flush() {
}

void NullConsoleMirrorSink::clear() {
}

void NullConsoleMirrorSink::refresh() {
}

void NullConsoleMirrorSink::setMode(lexxos::app::ConsoleMirrorMode mode) {
	_mode = mode;
}

lexxos::app::ConsoleMirrorMode NullConsoleMirrorSink::mode() const {
	return _mode;
}

bool NullConsoleMirrorSink::update() {
	return false;
}

bool NullConsoleMirrorSink::isReady() const {
	return false;
}

} // namespace lexxos::app::detail
