#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/runtime/selected_runtime_profile.h>

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

} // namespace lexxos::vfs
