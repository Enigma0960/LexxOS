#include <lexxos/vfs/vfs.h>

#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/detail/vfs_mount_table.h>
#include <lexxos/vfs/detail/vfs_pseudo_files.h>
#include <lexxos/vfs/detail/vfs_string.h>
#include <lexxos/vfs/detail/vfs_tmpfs.h>

lexxos::vfs::Vfs::Vfs()
    : _platformServices(&lexxos::platform::services()) {
}

void lexxos::vfs::Vfs::initialize() {
	initialize(lexxos::platform::services());
}

void lexxos::vfs::Vfs::initialize(lexxos::platform::PlatformServices const& platform) {
	initializePseudoFiles(platform);
	initializeMountTable();
	initializeTmpfs();
}

lexxos::platform::PlatformServices const& lexxos::vfs::Vfs::platformServices() const {
	return _platformServices != nullptr ? *_platformServices : lexxos::platform::services();
}

lexxos::vfs::Vfs& lexxos::vfs::defaultVfs() {
	static Vfs instance;
	return instance;
}

bool lexxos::vfs::Vfs::isDirectory(std::string_view path) {
	Stat output;
	return stat(path, output) && output.type == NodeType::Directory;
}

bool lexxos::vfs::Vfs::stat(std::string_view path, Stat& output) {
	output = {};

	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->stat(pathInsideMount(*mount, path), output);
	}
	if (statPseudoNode(path, output)) {
		return true;
	}
	if (statTmpFile(path, output)) {
		return true;
	}
	if (auto* mount = findBlockDeviceByPath(path); mount != nullptr) {
		output.type = NodeType::BlockDevice;
		if (mount->device != nullptr) {
			output.ready = mount->device->isReady();
			output.blockSizeBytes = mount->device->blockSizeBytes();
			output.blockCount = mount->device->blockCount();
			output.sizeBytes = output.blockCount * static_cast<std::uint64_t>(output.blockSizeBytes);
		}
		return true;
	}

	return false;
}

bool lexxos::vfs::Vfs::list(
    std::string_view path,
    DirectoryVisitor visitor,
    void* userData) {
	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->list(pathInsideMount(*mount, path), visitor, userData);
	}
	if (detail::equals(path, "/dev")) {
		return listBlockDevices(visitor, userData);
	}
	if (detail::equals(path, "/mnt")) {
		return listMountTargets(visitor, userData);
	}
	if (detail::equals(path, "/tmp")) {
		return listTmp(visitor, userData);
	}
	return listPseudoDirectory(path, visitor, userData);
}

lexxos::vfs::ReadStatus lexxos::vfs::Vfs::readText(
    std::string_view path,
    TextWriter writer,
    void* userData) {
	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->readText(pathInsideMount(*mount, path), writer, userData);
	}
	if (isKnownDirectory(path)) {
		return ReadStatus::IsDirectory;
	}
	if (findBlockDeviceByPath(path) != nullptr) {
		return ReadStatus::IsBlockDevice;
	}
	if (isTmpFilePath(path)) {
		return readTmpText(path, writer, userData);
	}
	return readPseudoText(path, writer, userData);
}

lexxos::vfs::RemoveStatus lexxos::vfs::Vfs::remove(std::string_view path) {
	if (path.empty() || path[0] != '/') {
		return RemoveStatus::InvalidPath;
	}
	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->remove(pathInsideMount(*mount, path));
	}
	if (isKnownDirectory(path)) {
		return RemoveStatus::IsDirectory;
	}
	if (findBlockDeviceByPath(path) != nullptr) {
		return RemoveStatus::IsBlockDevice;
	}
	if (isKnownStaticFile(path)) {
		return RemoveStatus::NotWritable;
	}
	return removeTmpFile(path);
}

lexxos::vfs::WriteStatus lexxos::vfs::Vfs::createDirectory(std::string_view path) {
	if (path.empty() || path[0] != '/') {
		return WriteStatus::InvalidPath;
	}

	Stat existing{};
	if (stat(path, existing)) {
		return existing.type == NodeType::Directory
		    ? WriteStatus::IsDirectory
		    : WriteStatus::NotWritable;
	}
	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->createDirectory(pathInsideMount(*mount, path));
	}
	if (isKnownDirectory(path)) {
		return WriteStatus::IsDirectory;
	}
	if (findBlockDeviceByPath(path) != nullptr) {
		return WriteStatus::IsBlockDevice;
	}
	return WriteStatus::NotWritable;
}

lexxos::vfs::RemoveStatus lexxos::vfs::Vfs::removeDirectory(std::string_view path) {
	if (path.empty() || path[0] != '/') {
		return RemoveStatus::InvalidPath;
	}
	if (auto* mount = findFilesystemByPath(path); mount != nullptr) {
		return mount->filesystem->removeDirectory(pathInsideMount(*mount, path));
	}
	if (isKnownDirectory(path)) {
		return RemoveStatus::NotWritable;
	}
	if (findBlockDeviceByPath(path) != nullptr) {
		return RemoveStatus::IsBlockDevice;
	}
	return RemoveStatus::NotFound;
}

lexxos::vfs::RenameStatus lexxos::vfs::Vfs::rename(
    std::string_view source,
    std::string_view target) {
	if (source.empty() || source[0] != '/' || target.empty() || target[0] != '/') {
		return RenameStatus::InvalidPath;
	}
	if (source.size() + 1 > maxPathLength || target.size() + 1 > maxPathLength) {
		return RenameStatus::InvalidPath;
	}
	if (detail::equals(source, target)) {
		return RenameStatus::Ok;
	}

	auto* sourceMount = findFilesystemByPath(source);
	auto* targetMount = findFilesystemByPath(target);
	if (sourceMount != nullptr || targetMount != nullptr) {
		if (sourceMount == nullptr || targetMount == nullptr || sourceMount != targetMount) {
			return RenameStatus::CrossFilesystem;
		}
		return sourceMount->filesystem->rename(
		    pathInsideMount(*sourceMount, source),
		    pathInsideMount(*targetMount, target));
	}
	if (isTmpFilePath(source) || isTmpFilePath(target)) {
		return renameTmpFile(source, target);
	}
	if (findBlockDeviceByPath(source) != nullptr
	    || findBlockDeviceByPath(target) != nullptr) {
		return RenameStatus::IsBlockDevice;
	}
	if (isKnownDirectory(source) || isKnownStaticFile(source)) {
		return RenameStatus::NotWritable;
	}
	return RenameStatus::SourceNotFound;
}

void lexxos::vfs::initialize() {
	defaultVfs().initialize();
}

void lexxos::vfs::initialize(lexxos::platform::PlatformServices const& platform) {
	defaultVfs().initialize(platform);
}

bool lexxos::vfs::isDirectory(std::string_view path) {
	return defaultVfs().isDirectory(path);
}

bool lexxos::vfs::stat(std::string_view path, Stat& output) {
	return defaultVfs().stat(path, output);
}

bool lexxos::vfs::list(std::string_view path, DirectoryVisitor visitor, void* userData) {
	return defaultVfs().list(path, visitor, userData);
}

lexxos::vfs::ReadStatus lexxos::vfs::readText(std::string_view path, TextWriter writer, void* userData) {
	return defaultVfs().readText(path, writer, userData);
}

lexxos::vfs::RemoveStatus lexxos::vfs::remove(std::string_view path) {
	return defaultVfs().remove(path);
}

lexxos::vfs::WriteStatus lexxos::vfs::createDirectory(std::string_view path) {
	return defaultVfs().createDirectory(path);
}

lexxos::vfs::RemoveStatus lexxos::vfs::removeDirectory(std::string_view path) {
	return defaultVfs().removeDirectory(path);
}

lexxos::vfs::RenameStatus lexxos::vfs::rename(std::string_view source, std::string_view target) {
	return defaultVfs().rename(source, target);
}
