#include <lexxos/io/fixed_buffer_input_stream.h>

#include <algorithm>
#include <cstring>

namespace lexxos::io {

FixedBufferInputStream::FixedBufferInputStream(std::string_view data)
    : _data(data) {
}

lexxos::vfs::ReadStatus FixedBufferInputStream::read(void* buffer, std::size_t capacity, std::size_t& bytesRead) {
	bytesRead = 0;
	if (capacity == 0) {
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (buffer == nullptr) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}

	if (_position >= _data.size()) {
		return lexxos::vfs::ReadStatus::Ok;
	}

	auto const available = _data.size() - _position;
	auto const count = std::min(available, capacity);
	std::memcpy(buffer, _data.data() + _position, count);
	_position += count;
	bytesRead = count;
	return lexxos::vfs::ReadStatus::Ok;
}

void FixedBufferInputStream::reset(std::string_view data) {
	_data = data;
	_position = 0;
}

} // namespace lexxos::io
