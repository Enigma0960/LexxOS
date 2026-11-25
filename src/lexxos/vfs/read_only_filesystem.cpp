#include <lexxos/vfs/read_only_filesystem.h>

namespace lexxos::vfs {

bool ReadOnlyFilesystem::isReadOnly() const {
	return true;
}

WriteStatus ReadOnlyFilesystem::writeText(std::string_view path, std::string_view text, WriteMode mode) {
	(void) path;
	(void) text;
	(void) mode;
	return WriteStatus::ReadOnlyFilesystem;
}

WriteStatus ReadOnlyFilesystem::beginWrite(std::string_view path, WriteMode mode) {
	return writeText(path, {}, mode);
}

WriteStatus ReadOnlyFilesystem::write(std::string_view path, std::uint64_t offset, std::string_view data) {
	(void) offset;
	return writeText(path, data, WriteMode::Append);
}

void ReadOnlyFilesystem::closeWrite(std::string_view path) {
	(void) path;
}

WriteStatus ReadOnlyFilesystem::finishWrite(std::string_view path) {
	closeWrite(path);
	return WriteStatus::Ok;
}

RemoveStatus ReadOnlyFilesystem::remove(std::string_view path) {
	(void) path;
	return RemoveStatus::ReadOnlyFilesystem;
}

WriteStatus ReadOnlyFilesystem::createDirectory(std::string_view path) {
	(void) path;
	return WriteStatus::ReadOnlyFilesystem;
}

RemoveStatus ReadOnlyFilesystem::removeDirectory(std::string_view path) {
	(void) path;
	return RemoveStatus::ReadOnlyFilesystem;
}

RenameStatus ReadOnlyFilesystem::rename(std::string_view source, std::string_view target) {
	(void) source;
	(void) target;
	return RenameStatus::ReadOnlyFilesystem;
}

bool ReadOnlyFilesystem::sync() {
	return isReady();
}

} // namespace lexxos::vfs
