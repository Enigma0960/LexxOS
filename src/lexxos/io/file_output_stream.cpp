#include <lexxos/io/file_output_stream.h>

#include <lexxos/vfs/vfs.h>

namespace lexxos::io {

FileOutputStream::FileOutputStream(std::string_view path, lexxos::vfs::WriteMode mode) {
	open(path, mode);
}

bool FileOutputStream::open(std::string_view path, lexxos::vfs::WriteMode mode) {
	// Повторное открытие сохраняет ошибку завершения предыдущей записи.
	auto const finishStatus = _file.finish();
	if (finishStatus != lexxos::vfs::WriteStatus::Ok) {
		_status = finishStatus;
		return false;
	}

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

bool FileOutputStream::finish() {
	auto const finishStatus = _file.finish();
	if (_status == lexxos::vfs::WriteStatus::Ok) {
		_status = finishStatus;
	}
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

} // namespace lexxos::io
