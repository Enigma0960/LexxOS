#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::vfs {

static constexpr std::size_t maxPathLength = lexxos::runtime::standardRuntimeProfile.vfs.maxPathLength;
static constexpr std::size_t maxNameLength = lexxos::runtime::standardRuntimeProfile.vfs.maxNameLength;

class FileHandle;
class ReadOnlyFilesystem;
class Vfs;

enum class NodeType : std::uint8_t {
	Missing,
	Directory,
	RegularFile,
	BlockDevice,
};

enum class ReadStatus : std::uint8_t {
	Ok,
	NotFound,
	IsDirectory,
	IsBlockDevice,
	NotReadable,
	InvalidPath,
};

enum class WriteMode : std::uint8_t {
	CreateTruncate,
	Append,
};

enum class WriteStatus : std::uint8_t {
	Ok,
	NotFound,
	IsDirectory,
	IsBlockDevice,
	ReadOnlyFilesystem,
	NotWritable,
	InvalidPath,
	NoSpace,
	TooLarge,
};

enum class OpenMode : std::uint8_t {
	Read,
	CreateTruncateWrite,
	AppendWrite,
};

enum class OpenStatus : std::uint8_t {
	Ok,
	NotFound,
	IsDirectory,
	IsBlockDevice,
	ReadOnlyFilesystem,
	NotReadable,
	NotWritable,
	InvalidPath,
	NoSpace,
};

enum class RemoveStatus : std::uint8_t {
	Ok,
	NotFound,
	IsDirectory,
	IsBlockDevice,
	DirectoryNotEmpty,
	ReadOnlyFilesystem,
	NotWritable,
	InvalidPath,
};

enum class RenameStatus : std::uint8_t {
	Ok,
	SourceNotFound,
	TargetExists,
	IsBlockDevice,
	ReadOnlyFilesystem,
	NotWritable,
	InvalidPath,
	CrossFilesystem,
	CrossDirectory,
};

enum class MountStatus : std::uint8_t {
	Ok,
	InvalidSource,
	InvalidTarget,
	SourceNotFound,
	SourceIsNotBlockDevice,
	TargetOutsideMountRoot,
	TargetBusy,
	NoMountSlot,
};

enum class SyncStatus : std::uint8_t {
	Ok,
	Failed,
};

enum class UnmountStatus : std::uint8_t {
	Ok,
	InvalidTarget,
	TargetNotMounted,
	SyncFailed,
};

using FilesystemFactory = MountStatus (*)(Vfs& vfs, std::string_view source, std::string_view target, void* userData);

// Metadata not supplied by a filesystem remains unset; never synthesize timestamps.
struct FileTimestamp {
	std::uint16_t year{0};
	std::uint8_t month{0};
	std::uint8_t day{0};
	std::uint8_t hour{0};
	std::uint8_t minute{0};
	std::uint8_t second{0};
	bool valid{false};
};

struct Stat {
	NodeType type{NodeType::Missing};
	std::uint64_t sizeBytes{0};
	std::uint32_t blockSizeBytes{0};
	std::uint64_t blockCount{0};
	bool ready{false};
	FileTimestamp createdAt{};
	FileTimestamp modifiedAt{};
	std::uint8_t fatAttributes{0};
	bool hasFatAttributes{false};
};

struct DirectoryEntry {
	std::string_view name;
	NodeType type{NodeType::Missing};
	std::uint64_t sizeBytes{0};
};

struct MountInfo {
	std::string_view source;
	std::string_view target;
	std::string_view type;
	bool ready{true};
	bool readOnly{true};
};

struct TmpInfo {
	std::size_t maxFiles{0};
	std::size_t usedFiles{0};
	std::size_t freeFiles{0};
	std::size_t maxFileSizeBytes{0};
	std::size_t totalBytes{0};
	std::size_t usedBytes{0};
	std::size_t freeBytes{0};
};

struct VfsInfo {
	std::size_t maxBlockDevices{0};
	std::size_t blockDevices{0};
	std::size_t maxFilesystemMounts{0};
	std::size_t filesystemMounts{0};
	TmpInfo tmp{};
};

using DirectoryVisitor = bool (*)(DirectoryEntry const& entry, void* userData);
using MountVisitor = bool (*)(MountInfo const& mount, void* userData);
using TextWriter = void (*)(std::string_view text, void* userData);

namespace detail {

	inline constexpr std::size_t maxBlockDevices = lexxos::runtime::standardRuntimeProfile.vfs.maxBlockDevices;
	inline constexpr std::size_t maxFilesystemMounts = lexxos::runtime::standardRuntimeProfile.vfs.maxFilesystemMounts;
	inline constexpr std::size_t maxTmpFiles = lexxos::runtime::standardRuntimeProfile.vfs.maxTmpFiles;
	inline constexpr std::size_t maxTmpFileSize = lexxos::runtime::standardRuntimeProfile.vfs.maxTmpFileSizeBytes;
	inline constexpr std::size_t maxFilesystemFactories = lexxos::runtime::standardRuntimeProfile.vfs.maxFilesystemFactories;

	struct FixedString {
		std::array<char, maxPathLength> data{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		bool append(char value) {
			if (size + 1 >= data.size()) {
				return false;
			}
			data[size++] = value;
			data[size] = '\0';
			return true;
		}

		bool append(std::string_view value) {
			for (char const character : value) {
				if (!append(character)) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] std::string_view view() const {
			return std::string_view{data.data(), size};
		}
	};

	struct BlockDeviceMount {
		bool used{false};
		std::array<char, maxNameLength> name{};
		std::array<char, maxPathLength> path{};
		lexxos::devices::BlockDevice* device{nullptr};
	};

	struct FilesystemMount {
		bool used{false};
		std::array<char, maxPathLength> source{};
		std::array<char, maxPathLength> path{};
		ReadOnlyFilesystem* filesystem{nullptr};
	};

	struct TmpFile {
		bool used{false};
		std::array<char, maxNameLength> name{};
		std::array<char, maxTmpFileSize> data{};
		std::size_t size{0};
	};

	struct FilesystemFactoryEntry {
		bool used{false};
		std::array<char, maxNameLength> type{};
		FilesystemFactory factory{nullptr};
		void* userData{nullptr};
	};

	struct TextWriterContext {
		TextWriter writer{nullptr};
		void* userData{nullptr};
	};

	struct TextSliceContext {
		char* output{nullptr};
		std::size_t capacity{0};
		std::uint64_t skip{0};
		std::uint64_t position{0};
		std::size_t copied{0};
	};

} // namespace detail

class FileHandle final {
public:
	FileHandle() = default;
	FileHandle(FileHandle const&) = delete;
	FileHandle& operator=(FileHandle const&) = delete;
	FileHandle(FileHandle&& other) noexcept;
	FileHandle& operator=(FileHandle&& other) noexcept;
	~FileHandle();

	[[nodiscard]] bool isOpen() const;
	[[nodiscard]] bool canRead() const;
	[[nodiscard]] bool canWrite() const;
	[[nodiscard]] std::string_view path() const;
	[[nodiscard]] std::uint64_t position() const;
	[[nodiscard]] std::uint64_t size() const;

	ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead);
	WriteStatus write(std::string_view data);
	bool seek(std::uint64_t position);
	void close();

private:
	friend OpenStatus open(std::string_view path, OpenMode mode, FileHandle& output);
	friend class Vfs;

	enum class Kind : std::uint8_t {
		None,
		DevNull,
		TmpFile,
		TextFile,
		FilesystemRead,
		FilesystemWrite,
	};

	void reset();
	bool setPath(std::string_view path);
	bool setRelativePath(std::string_view path);

	Kind _kind{Kind::None};
	std::array<char, maxPathLength> _path{};
	std::size_t _pathSize{0};
	std::array<char, maxPathLength> _relativePath{};
	std::size_t _relativePathSize{0};
	std::size_t _tmpIndex{0};
	Vfs* _vfs{nullptr};
	ReadOnlyFilesystem* _filesystem{nullptr};
	std::uint64_t _position{0};
	std::uint64_t _size{0};
	bool _readable{false};
	bool _writable{false};
	bool _open{false};
};

class ReadOnlyFilesystem {
public:
	virtual ~ReadOnlyFilesystem() = default;
	[[nodiscard]] virtual bool isReady() const = 0;
	[[nodiscard]] virtual std::string_view typeName() const = 0;

	[[nodiscard]] virtual bool isReadOnly() const { return true; }

	virtual bool stat(std::string_view path, Stat& output) = 0;
	virtual bool list(std::string_view path, DirectoryVisitor visitor, void* userData) = 0;
	virtual ReadStatus read(
	    std::string_view path,
	    std::uint64_t offset,
	    void* buffer,
	    std::size_t capacity,
	    std::size_t& bytesRead) = 0;
	virtual ReadStatus readText(std::string_view path, TextWriter writer, void* userData) = 0;

	virtual WriteStatus writeText(std::string_view path, std::string_view text, WriteMode mode) {
		(void) path;
		(void) text;
		(void) mode;
		return WriteStatus::ReadOnlyFilesystem;
	}

	virtual WriteStatus beginWrite(std::string_view path, WriteMode mode) {
		return writeText(path, {}, mode);
	}

	virtual WriteStatus write(std::string_view path, std::uint64_t offset, std::string_view data) {
		(void) offset;
		return writeText(path, data, WriteMode::Append);
	}

	virtual void closeWrite(std::string_view path) {
		(void) path;
	}

	virtual RemoveStatus remove(std::string_view path) {
		(void) path;
		return RemoveStatus::ReadOnlyFilesystem;
	}

	virtual WriteStatus createDirectory(std::string_view path) {
		(void) path;
		return WriteStatus::ReadOnlyFilesystem;
	}

	virtual RemoveStatus removeDirectory(std::string_view path) {
		(void) path;
		return RemoveStatus::ReadOnlyFilesystem;
	}

	virtual RenameStatus rename(std::string_view source, std::string_view target) {
		(void) source;
		(void) target;
		return RenameStatus::ReadOnlyFilesystem;
	}

	[[nodiscard]] virtual bool sync() {
		return isReady();
	}
};

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
