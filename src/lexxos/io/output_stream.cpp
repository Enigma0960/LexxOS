#include <lexxos/io/output_stream.h>

#include <algorithm>
#include <cstring>

#include <lexxos/app/console_output.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

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

lexxos::vfs::ReadStatus NullInputStream::read(void*, std::size_t, std::size_t& bytesRead) {
	bytesRead = 0;
	return lexxos::vfs::ReadStatus::Ok;
}

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

FileInputStream::FileInputStream(std::string_view path) {
	open(path);
}

bool FileInputStream::open(std::string_view path) {
	_file.close();
	_status = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, _file);
	return ok();
}

lexxos::vfs::ReadStatus FileInputStream::read(void* buffer, std::size_t capacity, std::size_t& bytesRead) {
	bytesRead = 0;
	if (!ok()) {
		switch (_status) {
			case lexxos::vfs::OpenStatus::Ok:
				break;
			case lexxos::vfs::OpenStatus::NotFound:
				return lexxos::vfs::ReadStatus::NotFound;
			case lexxos::vfs::OpenStatus::IsDirectory:
				return lexxos::vfs::ReadStatus::IsDirectory;
			case lexxos::vfs::OpenStatus::IsBlockDevice:
				return lexxos::vfs::ReadStatus::IsBlockDevice;
			case lexxos::vfs::OpenStatus::ReadOnlyFilesystem:
			case lexxos::vfs::OpenStatus::NotReadable:
			case lexxos::vfs::OpenStatus::NotWritable:
				return lexxos::vfs::ReadStatus::NotReadable;
			case lexxos::vfs::OpenStatus::InvalidPath:
			case lexxos::vfs::OpenStatus::NoSpace:
				return lexxos::vfs::ReadStatus::InvalidPath;
		}
	}
	return _file.read(buffer, capacity, bytesRead);
}

bool FileInputStream::ok() const {
	return _status == lexxos::vfs::OpenStatus::Ok;
}

lexxos::vfs::OpenStatus FileInputStream::status() const {
	return _status;
}

std::string_view FileInputStream::path() const {
	return _file.path();
}

bool OutputStream::writeLine(std::string_view text) {
	return write(text) && write(lineEnding());
}

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

bool NullOutputStream::write(std::string_view) {
	return true;
}

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

FileOutputStream::FileOutputStream(std::string_view path, lexxos::vfs::WriteMode mode) {
	open(path, mode);
}

bool FileOutputStream::open(std::string_view path, lexxos::vfs::WriteMode mode) {
	_file.close();
	_status = lexxos::vfs::WriteStatus::InvalidPath;

	auto const openMode = (mode == lexxos::vfs::WriteMode::CreateTruncate)
	    ? lexxos::vfs::OpenMode::CreateTruncateWrite
	    : lexxos::vfs::OpenMode::AppendWrite;

	auto const openStatus = lexxos::vfs::open(path, openMode, _file);
	switch (openStatus) {
		case lexxos::vfs::OpenStatus::Ok:
			_status = lexxos::vfs::WriteStatus::Ok;
			break;
		case lexxos::vfs::OpenStatus::NotFound:
			_status = lexxos::vfs::WriteStatus::NotFound;
			break;
		case lexxos::vfs::OpenStatus::IsDirectory:
			_status = lexxos::vfs::WriteStatus::IsDirectory;
			break;
		case lexxos::vfs::OpenStatus::IsBlockDevice:
			_status = lexxos::vfs::WriteStatus::IsBlockDevice;
			break;
		case lexxos::vfs::OpenStatus::ReadOnlyFilesystem:
			_status = lexxos::vfs::WriteStatus::ReadOnlyFilesystem;
			break;
		case lexxos::vfs::OpenStatus::NotReadable:
		case lexxos::vfs::OpenStatus::NotWritable:
			_status = lexxos::vfs::WriteStatus::NotWritable;
			break;
		case lexxos::vfs::OpenStatus::InvalidPath:
			_status = lexxos::vfs::WriteStatus::InvalidPath;
			break;
		case lexxos::vfs::OpenStatus::NoSpace:
			_status = lexxos::vfs::WriteStatus::NoSpace;
			break;
			return ok();
	}
	return ok();
}

bool FileOutputStream::write(std::string_view text) {
	if (!ok()) {
		return false;
	}
	_status = _file.write(text);
	return ok();
}

bool FileOutputStream::ok() const {
	return _status == lexxos::vfs::WriteStatus::Ok;
}

lexxos::vfs::WriteStatus FileOutputStream::status() const {
	return _status;
}

std::string_view FileOutputStream::path() const {
	return _file.path();
}

InputStream& nullInputStream() {
	static NullInputStream stream;
	return stream;
}

OutputStream& consoleOutputStream() {
	static ConsoleOutputStream stream;
	return stream;
}

OutputStream& nullOutputStream() {
	static NullOutputStream stream;
	return stream;
}

} // namespace lexxos::io
