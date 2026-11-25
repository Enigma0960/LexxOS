#include <lexxos/io/file_input_stream.h>

#include <lexxos/vfs/vfs.h>

namespace lexxos::io {

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
			case lexxos::vfs::OpenStatus::NotReadable       :
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

} // namespace lexxos::io
