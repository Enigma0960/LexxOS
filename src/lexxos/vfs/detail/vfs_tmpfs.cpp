#include <lexxos/vfs/detail/vfs_tmpfs.h>

#include <cstring>

#include <lexxos/vfs/detail/vfs_string.h>

void lexxos::vfs::Vfs::initializeTmpfs() {
	for (auto& file : _tmpFiles) {
		file = {};
	}
}

bool lexxos::vfs::Vfs::isTmpFilePath(std::string_view path) const {
	constexpr std::string_view prefix{"/tmp/"};
	if (!detail::startsWith(path, prefix) || path.size() <= prefix.size()) {
		return false;
	}
	for (std::size_t index = prefix.size(); index < path.size(); ++index) {
		if (path[index] == '/') {
			return false;
		}
	}
	return true;
}

std::string_view lexxos::vfs::Vfs::tmpFileName(std::string_view path) const {
	constexpr std::string_view prefix{"/tmp/"};
	return path.substr(prefix.size());
}

lexxos::vfs::detail::TmpFile* lexxos::vfs::Vfs::findTmpFileByName(std::string_view name) {
	for (auto& file : _tmpFiles) {
		if (file.used && detail::equals(file.name.data(), name)) {
			return &file;
		}
	}
	return nullptr;
}

lexxos::vfs::detail::TmpFile* lexxos::vfs::Vfs::tmpFileByIndex(std::size_t index) {
	if (index >= _tmpFiles.size() || !_tmpFiles[index].used) {
		return nullptr;
	}
	return &_tmpFiles[index];
}

std::size_t lexxos::vfs::Vfs::tmpFileIndex(detail::TmpFile const* target) const {
	for (std::size_t index = 0; index < _tmpFiles.size(); ++index) {
		if (&_tmpFiles[index] == target) {
			return index;
		}
	}
	return _tmpFiles.size();
}

lexxos::vfs::detail::TmpFile* lexxos::vfs::Vfs::ensureTmpFile(std::string_view name) {
	if (name.empty() || name.size() + 1 > maxNameLength) {
		return nullptr;
	}
	if (auto* existing = findTmpFileByName(name); existing != nullptr) {
		return existing;
	}
	for (auto& file : _tmpFiles) {
		if (file.used) {
			continue;
		}
		if (!detail::copyText(name, file.name.data(), file.name.size())) {
			return nullptr;
		}
		file.used = true;
		file.size = 0;
		return &file;
	}
	return nullptr;
}

bool lexxos::vfs::Vfs::statTmpFile(std::string_view path, Stat& output) {
	if (!isTmpFilePath(path)) {
		return false;
	}
	auto const name = tmpFileName(path);
	auto const* file = findTmpFileByName(name);
	if (file == nullptr) {
		return false;
	}

	output.type = NodeType::RegularFile;
	output.ready = true;
	output.sizeBytes = file->size;
	return true;
}

bool lexxos::vfs::Vfs::listTmp(DirectoryVisitor visitor, void* userData) {
	for (auto const& file : _tmpFiles) {
		if (!file.used) {
			continue;
		}
		if (!detail::visitEntry(visitor, userData, file.name.data(), NodeType::RegularFile, file.size)) {
			return false;
		}
	}
	return true;
}

lexxos::vfs::ReadStatus lexxos::vfs::Vfs::readTmpText(
    std::string_view path,
    TextWriter writer,
    void* userData) {
	if (!isTmpFilePath(path)) {
		return ReadStatus::NotFound;
	}
	auto const name = tmpFileName(path);
	auto const* file = findTmpFileByName(name);
	if (file == nullptr) {
		return ReadStatus::NotFound;
	}
	writer(std::string_view{file->data.data(), file->size}, userData);
	return ReadStatus::Ok;
}

lexxos::vfs::RemoveStatus lexxos::vfs::Vfs::removeTmpFile(std::string_view path) {
	if (!isTmpFilePath(path)) {
		return RemoveStatus::NotWritable;
	}

	auto const name = tmpFileName(path);
	auto* file = findTmpFileByName(name);
	if (file == nullptr) {
		return RemoveStatus::NotFound;
	}

	*file = {};
	return RemoveStatus::Ok;
}

lexxos::vfs::RenameStatus lexxos::vfs::Vfs::renameTmpFile(
    std::string_view source,
    std::string_view target) {
	if (!isTmpFilePath(source) || !isTmpFilePath(target)) {
		return RenameStatus::CrossFilesystem;
	}
	auto const sourceName = tmpFileName(source);
	auto const targetName = tmpFileName(target);
	if (sourceName.empty() || targetName.empty() || targetName.size() + 1 > maxNameLength) {
		return RenameStatus::InvalidPath;
	}
	if (detail::equals(sourceName, targetName)) {
		return RenameStatus::Ok;
	}
	auto* sourceFile = findTmpFileByName(sourceName);
	if (sourceFile == nullptr) {
		return RenameStatus::SourceNotFound;
	}
	if (findTmpFileByName(targetName) != nullptr) {
		return RenameStatus::TargetExists;
	}
	return detail::copyText(targetName, sourceFile->name.data(), sourceFile->name.size())
	    ? RenameStatus::Ok
	    : RenameStatus::InvalidPath;
}

lexxos::vfs::TmpInfo lexxos::vfs::Vfs::tmpInfo() {
	TmpInfo info{
	    .maxFiles = detail::maxTmpFiles,
	    .usedFiles = 0,
	    .freeFiles = 0,
	    .maxFileSizeBytes = detail::maxTmpFileSize,
	    .totalBytes = detail::maxTmpFiles * detail::maxTmpFileSize,
	    .usedBytes = 0,
	    .freeBytes = 0,
	};

	for (auto const& file : _tmpFiles) {
		if (file.used) {
			++info.usedFiles;
			info.usedBytes += file.size;
		} else {
			++info.freeFiles;
		}
	}

	info.freeBytes = info.totalBytes > info.usedBytes ? info.totalBytes - info.usedBytes : 0;
	return info;
}

lexxos::vfs::TmpInfo lexxos::vfs::tmpInfo() {
	return defaultVfs().tmpInfo();
}
