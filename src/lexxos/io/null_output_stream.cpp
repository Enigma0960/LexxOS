#include <lexxos/io/null_output_stream.h>

namespace lexxos::io {

bool NullOutputStream::write(std::string_view) {
	return true;
}

OutputStream& nullOutputStream() {
	static NullOutputStream stream;
	return stream;
}

} // namespace lexxos::io
