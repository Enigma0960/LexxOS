#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/runtime/selected_runtime_profile.h>
#include <lexxos/vfs/detail/vfs_types.h>
#include <lexxos/vfs/file_handle.h>
#include <lexxos/vfs/read_only_filesystem.h>
#include <lexxos/vfs/vfs_types.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::vfs {

// namespace detail

class Vfs final {
public:
	Vfs();

	void initialize();
	void initialize(lexxos::platform::PlatformServices const& platform);
	bool normalizePath(std::string_view cwd, std::string_view path, char* output, std::size_t outputCapacity);
	[[nodiscard]] bool isDirectory(std::string_view path);
	[[nodiscard]] bool stat(std::string_view path, Stat& output);
	bool list(std::string_view path, DirectoryVisitor visitor, void* userData);
	OpenStatus open(std::string_view path, OpenMode mode, FileHandle& output);
	WriteStatus writeText(std::string_view path, std::string_view text, WriteMode mode = WriteMode::CreateTruncate);
	RemoveStatus remove(std::string_view path);
	WriteStatus createDirectory(std::string_view path);
	RemoveStatus removeDirectory(std::string_view path);
	RenameStatus rename(std::string_view source, std::string_view target);
	SyncStatus sync();
	ReadStatus readText(std::string_view path, TextWriter writer, void* userData);
	bool registerBlockDevice(std::string_view name, lexxos::devices::BlockDevice* device);
	[[nodiscard]] lexxos::devices::BlockDevice* blockDevice(std::string_view path);
	MountStatus mountFilesystem(std::string_view source, std::string_view target, ReadOnlyFilesystem* filesystem);
	UnmountStatus unmountFilesystem(std::string_view target);
	UnmountStatus forceUnmountFilesystem(std::string_view target);
	[[nodiscard]] bool isMountTarget(std::string_view target);
	bool visitMounts(MountVisitor visitor, void* userData);
	[[nodiscard]] TmpInfo tmpInfo();
	[[nodiscard]] VfsInfo vfsInfo();
	bool registerFilesystemFactory(std::string_view type, FilesystemFactory factory, void* userData = nullptr);
	MountStatus mount(std::string_view type, std::string_view source, std::string_view target);

private:
	friend class FileHandle;

	void initializeMountTable();
	void initializeTmpfs();
	void initializePseudoFiles(lexxos::platform::PlatformServices const& platform);

	[[nodiscard]] lexxos::platform::PlatformServices const& platformServices() const;

	detail::BlockDeviceMount* findBlockDeviceByPath(std::string_view path);
	detail::FilesystemMount* findFilesystemByPath(std::string_view path);
	std::string_view pathInsideMount(detail::FilesystemMount const& mount, std::string_view path);
	[[nodiscard]] bool listBlockDevices(DirectoryVisitor visitor, void* userData);
	[[nodiscard]] bool listMountTargets(DirectoryVisitor visitor, void* userData);

	[[nodiscard]] bool isTmpFilePath(std::string_view path) const;
	[[nodiscard]] std::string_view tmpFileName(std::string_view path) const;
	detail::TmpFile* findTmpFileByName(std::string_view name);
	detail::TmpFile* tmpFileByIndex(std::size_t index);
	std::size_t tmpFileIndex(detail::TmpFile const* target) const;
	detail::TmpFile* ensureTmpFile(std::string_view name);
	[[nodiscard]] bool statTmpFile(std::string_view path, Stat& output);
	[[nodiscard]] bool listTmp(DirectoryVisitor visitor, void* userData);
	ReadStatus readTmpText(std::string_view path, TextWriter writer, void* userData);
	RemoveStatus removeTmpFile(std::string_view path);
	RenameStatus renameTmpFile(std::string_view source, std::string_view target);

	[[nodiscard]] bool isKnownDirectory(std::string_view path) const;
	[[nodiscard]] bool isKnownStaticFile(std::string_view path) const;
	[[nodiscard]] bool statPseudoNode(std::string_view path, Stat& output);
	[[nodiscard]] bool listPseudoDirectory(std::string_view path, DirectoryVisitor visitor, void* userData);
	ReadStatus readPseudoText(std::string_view path, TextWriter writer, void* userData);

	detail::FilesystemFactoryEntry* findFilesystemFactory(std::string_view type);

	lexxos::platform::PlatformServices const* _platformServices{nullptr};
	std::array<detail::BlockDeviceMount, detail::maxBlockDevices> _blockDevices{};
	std::array<detail::FilesystemMount, detail::maxFilesystemMounts> _filesystems{};
	std::array<detail::TmpFile, detail::maxTmpFiles> _tmpFiles{};
	std::array<detail::FilesystemFactoryEntry, detail::maxFilesystemFactories> _filesystemFactories{};
};

[[nodiscard]] Vfs& defaultVfs();
void initialize();
void initialize(lexxos::platform::PlatformServices const& platform);
bool normalizePath(std::string_view cwd, std::string_view path, char* output, std::size_t outputCapacity);
[[nodiscard]] bool isDirectory(std::string_view path);
[[nodiscard]] bool stat(std::string_view path, Stat& output);
bool list(std::string_view path, DirectoryVisitor visitor, void* userData);
OpenStatus open(std::string_view path, OpenMode mode, FileHandle& output);
WriteStatus writeText(std::string_view path, std::string_view text, WriteMode mode = WriteMode::CreateTruncate);
RemoveStatus remove(std::string_view path);
WriteStatus createDirectory(std::string_view path);
RemoveStatus removeDirectory(std::string_view path);
RenameStatus rename(std::string_view source, std::string_view target);
SyncStatus sync();
ReadStatus readText(std::string_view path, TextWriter writer, void* userData);
bool registerBlockDevice(std::string_view name, lexxos::devices::BlockDevice* device);
[[nodiscard]] lexxos::devices::BlockDevice* blockDevice(std::string_view path);
MountStatus mountFilesystem(std::string_view source, std::string_view target, ReadOnlyFilesystem* filesystem);
UnmountStatus unmountFilesystem(std::string_view target);
UnmountStatus forceUnmountFilesystem(std::string_view target);
[[nodiscard]] bool isMountTarget(std::string_view target);
bool visitMounts(MountVisitor visitor, void* userData);
[[nodiscard]] TmpInfo tmpInfo();
[[nodiscard]] VfsInfo vfsInfo();
bool registerFilesystemFactory(std::string_view type, FilesystemFactory factory, void* userData = nullptr);
MountStatus mount(std::string_view type, std::string_view source, std::string_view target);

std::string_view toString(NodeType type);
std::string_view toString(ReadStatus status);
std::string_view toString(WriteStatus status);
std::string_view toString(OpenStatus status);
std::string_view toString(RemoveStatus status);
std::string_view toString(RenameStatus status);
std::string_view toString(SyncStatus status);
std::string_view toString(MountStatus status);
std::string_view toString(UnmountStatus status);

} // namespace lexxos::vfs
