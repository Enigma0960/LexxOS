#include <lexxos/io/console_output_stream.h>

#include <lexxos/app/console_output.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::io {

ConsoleOutputStream::ConsoleOutputStream()
    : ConsoleOutputStream(lexxos::platform::services()) {
}

ConsoleOutputStream::ConsoleOutputStream(lexxos::platform::PlatformServices const& platform)
    : _platform(&platform) {
}

bool ConsoleOutputStream::write(std::string_view text) {
	if (_platform == nullptr) {
		return true;
	}
	return lexxos::app::console_output::writeNormalized(*_platform, text, _lastWasCarriageReturn);
}

OutputStream& consoleOutputStream() {
	static ConsoleOutputStream stream;
	return stream;
}

} // namespace lexxos::io
