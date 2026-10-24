#include <lexxos/vfs/vfs.h>

#include <algorithm>
#include <cstring>

#include <lexxos/vfs/detail/vfs_mount_table.h>
#include <lexxos/vfs/detail/vfs_pseudo_files.h>
#include <lexxos/vfs/detail/vfs_string.h>
#include <lexxos/vfs/detail/vfs_tmpfs.h>
#include <lexxos/vfs/vfs_internal_types.h>

lexxos::vfs::FileHandle::FileHandle(FileHandle&& other) noexcept {
	*this = static_cast<FileHandle&&>(other);
}

lexxos::vfs::FileHandle& lexxos::vfs::FileHandle::operator=(FileHandle&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	close();
	_kind = other._kind;
	_path = other._path;
	_pathSize = other._pathSize;
	_relativePath = other._relativePath;
	_relativePathSize = other._relativePathSize;
	_tmpIndex = other._tmpIndex;
	_vfs = other._vfs;
	_filesystem = other._filesystem;
	_position = other._position;
	_size = other._size;
	_readable = other._readable;
	_writable = other._writable;
	_open = other._open;
	other.reset();
	return *this;
}

lexxos::vfs::FileHandle::~FileHandle() {
	close();
}

bool lexxos::vfs::FileHandle::isOpen() const {
	return _open;
}

bool lexxos::vfs::FileHandle::canRead() const {
	return _open && _readable;
}

bool lexxos::vfs::FileHandle::canWrite() const {
	return _open && _writable;
}

std::string_view lexxos::vfs::FileHandle::path() const {
	return std::string_view{_path.data(), _pathSize};
}

std::uint64_t lexxos::vfs::FileHandle::position() const {
	return _position;
}

std::uint64_t lexxos::vfs::FileHandle::size() const {
	if (_kind == Kind::TmpFile) {
		if (_vfs == nullptr) {
			return _size;
		}
		if (auto const* file = _vfs->tmpFileByIndex(_tmpIndex); file != nullptr) {
			return file->size;
		}
	}
	return _size;
}

lexxos::vfs::ReadStatus lexxos::vfs::FileHandle::read(
    void* buffer,
    std::size_t capacity,
    std::size_t& bytesRead) {
	bytesRead = 0;
	if (!_open || !_readable) {
		return ReadStatus::NotReadable;
	}
	if (capacity == 0) {
		return ReadStatus::Ok;
	}
	if (buffer == nullptr) {
		return ReadStatus::InvalidPath;
	}
	if (_kind == Kind::DevNull) {
		return ReadStatus::Ok;
	}
	if (_kind == Kind::TmpFile) {
		if (_vfs == nullptr) {
			return ReadStatus::NotFound;
		}
		auto const* file = _vfs->tmpFileByIndex(_tmpIndex);
		if (file == nullptr) {
			return ReadStatus::NotFound;
		}
		if (_position >= file->size) {
			return ReadStatus::Ok;
		}

		auto const available = file->size - static_cast<std::size_t>(_position);
		auto const count = std::min(available, capacity);
		std::memcpy(buffer, file->data.data() + _position, count);
		_position += count;
		bytesRead = count;
		return ReadStatus::Ok;
	}

	if (_kind == Kind::FilesystemRead) {
		if (_filesystem == nullptr) {
			return ReadStatus::NotReadable;
		}
		auto const status = _filesystem->read(
		    std::string_view{_relativePath.data(), _relativePathSize},
		    _position,
		    buffer,
		    capacity,
		    bytesRead);
		if (status == ReadStatus::Ok) {
			_position += bytesRead;
		}
		return status;
	}

	if (_kind == Kind::TextFile) {
		detail::TextSliceContext target{
		    .output = static_cast<char*>(buffer),
		    .capacity = capacity,
		    .skip = _position,
		};

		auto& vfs = _vfs != nullptr ? *_vfs : defaultVfs();
		auto const status = vfs.readText(
		    path(),
		    [](std::string_view text, void* userData) {
			    auto& slice = *static_cast<detail::TextSliceContext*>(userData);
			    if (slice.copied >= slice.capacity) {
				    slice.position += text.size();
				    return;
			    }

			    std::uint64_t const chunkBegin = slice.position;
			    std::uint64_t const chunkEnd = slice.position + text.size();
			    slice.position = chunkEnd;
			    if (chunkEnd <= slice.skip) {
				    return;
			    }

			    std::size_t const sourceOffset = chunkBegin < slice.skip
			        ? static_cast<std::size_t>(slice.skip - chunkBegin)
			        : 0;
			    std::size_t const sourceAvailable = text.size() > sourceOffset
			        ? text.size() - sourceOffset
			        : 0;
			    std::size_t const targetAvailable = slice.capacity - slice.copied;
			    std::size_t const count = std::min(sourceAvailable, targetAvailable);
			    if (count != 0) {
				    std::memcpy(slice.output + slice.copied, text.data() + sourceOffset, count);
				    slice.copied += count;
			    }
		    },
		    &target);
		if (status != ReadStatus::Ok) {
			return status;
		}

		_position += target.copied;
		bytesRead = target.copied;
		if (target.position > _size) {
			_size = target.position;
		}
		return ReadStatus::Ok;
	}

	return ReadStatus::NotReadable;
}

lexxos::vfs::WriteStatus lexxos::vfs::FileHandle::write(std::string_view data) {
	if (!_open || !_writable) {
		return WriteStatus::NotWritable;
	}
	if (_kind == Kind::DevNull) {
		_position += data.size();
		return WriteStatus::Ok;
	}
	if (_kind == Kind::FilesystemWrite) {
		if (_filesystem == nullptr) {
			return WriteStatus::NotWritable;
		}

		auto const status = _filesystem->write(
		    std::string_view{_relativePath.data(), _relativePathSize},
		    _position,
		    data);
		if (status != WriteStatus::Ok) {
			return status;
		}
		_position += data.size();
		if (_position > _size) {
			_size = _position;
		}
		return WriteStatus::Ok;
	}
	if (_kind != Kind::TmpFile) {
		return WriteStatus::NotWritable;
	}

	if (_vfs == nullptr) {
		return WriteStatus::NotWritable;
	}
	auto* file = _vfs->tmpFileByIndex(_tmpIndex);
	if (file == nullptr) {
		return WriteStatus::NotWritable;
	}
	if (_position > file->size) {
		return WriteStatus::InvalidPath;
	}
	if (data.size() > file->data.size() - static_cast<std::size_t>(_position)) {
		return WriteStatus::TooLarge;
	}

	if (!data.empty()) {
		std::memcpy(file->data.data() + _position, data.data(), data.size());
	}
	_position += data.size();
	if (_position > file->size) {
		file->size = static_cast<std::size_t>(_position);
	}
	_size = file->size;
	return WriteStatus::Ok;
}

bool lexxos::vfs::FileHandle::seek(std::uint64_t position) {
	if (!_open) {
		return false;
	}
	if (_kind == Kind::TmpFile && position > size()) {
		return false;
	}
	_position = position;
	return true;
}

void lexxos::vfs::FileHandle::close() {
	if (_open && _kind == Kind::FilesystemWrite && _filesystem != nullptr) {
		_filesystem->closeWrite(std::string_view{_relativePath.data(), _relativePathSize});
	}
	reset();
}

void lexxos::vfs::FileHandle::reset() {
	_kind = Kind::None;
	_path = {};
	_pathSize = 0;
	_relativePath = {};
	_relativePathSize = 0;
	_tmpIndex = 0;
	_vfs = nullptr;
	_filesystem = nullptr;
	_position = 0;
	_size = 0;
	_readable = false;
	_writable = false;
	_open = false;
}

bool lexxos::vfs::FileHandle::setPath(std::string_view path) {
	if (path.size() + 1 > _path.size()) {
		return false;
	}
	std::memcpy(_path.data(), path.data(), path.size());
	_path[path.size()] = '\0';
	_pathSize = path.size();
	return true;
}

bool lexxos::vfs::FileHandle::setRelativePath(std::string_view path) {
	if (path.size() + 1 > _relativePath.size()) {
		return false;
	}
	std::memcpy(_relativePath.data(), path.data(), path.size());
	_relativePath[path.size()] = '\0';
	_relativePathSize = path.size();
	return true;
}

lexxos::vfs::OpenStatus lexxos::vfs::Vfs::open(
    std::string_view path,
    OpenMode mode,
    FileHandle& output) {
	output.close();
	if (path.empty() || path[0] != '/' || path.size() + 1 > maxPathLength) {
		return OpenStatus::InvalidPath;
	}
	output._vfs = this;

	if (mode == OpenMode::Read) {
		Stat node;
		if (!stat(path, node)) {
			return OpenStatus::NotFound;
		}
		if (node.type == NodeType::Directory) {
			return OpenStatus::IsDirectory;
		}
		if (node.type == NodeType::BlockDevice) {
			return OpenStatus::IsBlockDevice;
		}

		if (!output.setPath(path)) {
			return OpenStatus::InvalidPath;
		}
		output._readable = true;
		output._writable = false;
		output._open = true;
		output._position = 0;
		output._size = node.sizeBytes;

		if (detail::equals(path, "/dev/null")) {
			output._kind = FileHandle::Kind::DevNull;
			return OpenStatus::Ok;
		}

		if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
			std::string_view const relativePath = pathInsideMount(*mount, path);
			if (!output.setRelativePath(relativePath)) {
				output.close();
				return OpenStatus::InvalidPath;
			}
			output._kind = FileHandle::Kind::FilesystemRead;
			output._filesystem = mount->filesystem;
			return OpenStatus::Ok;
		}

		if (isTmpFilePath(path)) {
			auto const name = tmpFileName(path);
			auto* file = findTmpFileByName(name);
			if (file == nullptr) {
				output.close();
				return OpenStatus::NotFound;
			}
			output._kind = FileHandle::Kind::TmpFile;
			output._tmpIndex = tmpFileIndex(file);
			output._size = file->size;
			return OpenStatus::Ok;
		}

		output._kind = FileHandle::Kind::TextFile;
		return OpenStatus::Ok;
	}

	if (detail::equals(path, "/dev/null")) {
		if (!output.setPath(path)) {
			return OpenStatus::InvalidPath;
		}
		output._kind = FileHandle::Kind::DevNull;
		output._readable = false;
		output._writable = true;
		output._open = true;
		return OpenStatus::Ok;
	}

	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		std::string_view const relativePath = pathInsideMount(*mount, path);
		Stat node{};
		bool const appendExistingFile = mode == OpenMode::AppendWrite
		    && mount->filesystem->stat(relativePath, node)
		    && node.type == NodeType::RegularFile;
		WriteStatus const prepareStatus = mount->filesystem->beginWrite(
		    relativePath,
		    mode == OpenMode::CreateTruncateWrite ? WriteMode::CreateTruncate : WriteMode::Append);
		switch (prepareStatus) {
			case WriteStatus::Ok:
				break;
			case WriteStatus::NotFound:
				return OpenStatus::NotFound;
			case WriteStatus::IsDirectory:
				return OpenStatus::IsDirectory;
			case WriteStatus::IsBlockDevice:
				return OpenStatus::IsBlockDevice;
			case WriteStatus::ReadOnlyFilesystem:
				return OpenStatus::ReadOnlyFilesystem;
			case WriteStatus::NotWritable:
				return OpenStatus::NotWritable;
			case WriteStatus::InvalidPath:
				return OpenStatus::InvalidPath;
			case WriteStatus::NoSpace:
				return OpenStatus::NoSpace;
			case WriteStatus::TooLarge:
				return OpenStatus::NoSpace;
		}

		if (!output.setPath(path) || !output.setRelativePath(relativePath)) {
			mount->filesystem->closeWrite(relativePath);
			return OpenStatus::InvalidPath;
		}
		output._kind = FileHandle::Kind::FilesystemWrite;
		output._filesystem = mount->filesystem;
		output._readable = false;
		output._writable = true;
		output._open = true;
		output._size = appendExistingFile ? node.sizeBytes : 0;
		output._position = mode == OpenMode::AppendWrite && appendExistingFile ? node.sizeBytes : 0;
		return OpenStatus::Ok;
	}
	if (isKnownDirectory(path)) {
		return OpenStatus::IsDirectory;
	}
	if (findBlockDeviceByPath(path) != nullptr) {
		return OpenStatus::IsBlockDevice;
	}
	if (!isTmpFilePath(path)) {
		return OpenStatus::NotWritable;
	}

	auto const name = tmpFileName(path);
	auto* file = ensureTmpFile(name);
	if (file == nullptr) {
		return name.size() + 1 > maxNameLength ? OpenStatus::InvalidPath : OpenStatus::NoSpace;
	}
	if (mode == OpenMode::CreateTruncateWrite) {
		file->size = 0;
	}

	if (!output.setPath(path)) {
		return OpenStatus::InvalidPath;
	}
	output._kind = FileHandle::Kind::TmpFile;
	output._tmpIndex = tmpFileIndex(file);
	output._readable = false;
	output._writable = true;
	output._open = true;
	output._size = file->size;
	output._position = mode == OpenMode::AppendWrite ? file->size : 0;
	return OpenStatus::Ok;
}

lexxos::vfs::OpenStatus lexxos::vfs::open(
    std::string_view path,
    OpenMode mode,
    FileHandle& output) {
	return defaultVfs().open(path, mode, output);
}

lexxos::vfs::WriteStatus lexxos::vfs::Vfs::writeText(
    std::string_view path,
    std::string_view text,
    WriteMode mode) {
	if (path.empty() || path[0] != '/' || path.size() + 1 > maxPathLength) {
		return WriteStatus::InvalidPath;
	}

	if (auto* mount = findFilesystemByPath(path); mount != nullptr && mount->filesystem != nullptr) {
		return mount->filesystem->writeText(pathInsideMount(*mount, path), text, mode);
	}

	FileHandle file;
	auto const openMode = mode == WriteMode::CreateTruncate
	    ? OpenMode::CreateTruncateWrite
	    : OpenMode::AppendWrite;
	auto const openStatus = open(path, openMode, file);
	switch (openStatus) {
		case OpenStatus::Ok:
			break;
		case OpenStatus::NotFound:
			return WriteStatus::NotFound;
		case OpenStatus::IsDirectory:
			return WriteStatus::IsDirectory;
		case OpenStatus::IsBlockDevice:
			return WriteStatus::IsBlockDevice;
		case OpenStatus::ReadOnlyFilesystem:
			return WriteStatus::ReadOnlyFilesystem;
		case OpenStatus::NotReadable:
			return WriteStatus::NotWritable;
		case OpenStatus::NotWritable:
			return WriteStatus::NotWritable;
		case OpenStatus::InvalidPath:
			return WriteStatus::InvalidPath;
		case OpenStatus::NoSpace:
			return WriteStatus::NoSpace;
	}

	return file.write(text);
}

lexxos::vfs::WriteStatus lexxos::vfs::writeText(
    std::string_view path,
    std::string_view text,
    WriteMode mode) {
	return defaultVfs().writeText(path, text, mode);
}
