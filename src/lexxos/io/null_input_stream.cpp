#include <lexxos/io/null_input_stream.h>

namespace lexxos::io {

lexxos::vfs::ReadStatus NullInputStream::read(void*, std::size_t, std::size_t& bytesRead) {
	bytesRead = 0;
	return lexxos::vfs::ReadStatus::Ok;
}

InputStream& nullInputStream() {
	static NullInputStream stream;
	return stream;
}

} // namespace lexxos::io
