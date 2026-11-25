#include <lexxos/io/fixed_buffer_output_stream.h>

#include <algorithm>
#include <cstring>

namespace lexxos::io {

FixedBufferOutputStream::FixedBufferOutputStream(char* buffer, std::size_t capacity)
    : _buffer(buffer)
    , _capacity(capacity) {
	reset();
}

bool FixedBufferOutputStream::write(std::string_view text) {
	if (_buffer == nullptr || _capacity == 0) {
		_overflowed = true;
		return false;
	}

	auto const available = _capacity > _size ? _capacity - _size : 0;
	auto const writable = std::min(available, text.size());
	if (writable != 0) {
		std::memcpy(_buffer + _size, text.data(), writable);
		_size += writable;
	}

	if (writable != text.size()) {
		_overflowed = true;
		return false;
	}

	return true;
}

void FixedBufferOutputStream::reset() {
	_size = 0;
	_overflowed = false;
}

std::string_view FixedBufferOutputStream::view() const {
	return std::string_view{_buffer, _size};
}

bool FixedBufferOutputStream::overflowed() const {
	return _overflowed;
}

} // namespace lexxos::io
