#include <lexxos/io/output_stream.h>

namespace {

std::string_view lineEnding() {
#if __APPLE__
	return "\n";
#else
	return "\r\n";
#endif
}

} // namespace

namespace lexxos::io {

bool OutputStream::writeLine(std::string_view text) {
	return write(text) && write(lineEnding());
}

} // namespace lexxos::io
