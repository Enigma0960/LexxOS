#include <lexxos/vfs/vfs.h>

std::string_view lexxos::vfs::toString(OpenStatus status) {
	switch (status) {
		case OpenStatus::Ok:
			return "ok";
		case OpenStatus::NotFound:
			return "not found";
		case OpenStatus::IsDirectory:
			return "is a directory";
		case OpenStatus::IsBlockDevice:
			return "is a block device";
		case OpenStatus::ReadOnlyFilesystem:
			return "read-only filesystem";
		case OpenStatus::NotReadable:
			return "not readable";
		case OpenStatus::NotWritable:
			return "not writable";
		case OpenStatus::InvalidPath:
			return "invalid path";
		case OpenStatus::NoSpace:
			return "no space";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(NodeType type) {
	switch (type) {
		case NodeType::Missing:
			return "missing";
		case NodeType::Directory:
			return "dir";
		case NodeType::RegularFile:
			return "file";
		case NodeType::BlockDevice:
			return "block";
	}
	return "unknown";
}

std::string_view lexxos::vfs::toString(ReadStatus status) {
	switch (status) {
		case ReadStatus::Ok:
			return "ok";
		case ReadStatus::NotFound:
			return "not found";
		case ReadStatus::IsDirectory:
			return "is a directory";
		case ReadStatus::IsBlockDevice:
			return "is a block device";
		case ReadStatus::NotReadable:
			return "not readable";
		case ReadStatus::InvalidPath:
			return "invalid path";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(WriteStatus status) {
	switch (status) {
		case WriteStatus::Ok:
			return "ok";
		case WriteStatus::NotFound:
			return "not found";
		case WriteStatus::IsDirectory:
			return "is a directory";
		case WriteStatus::IsBlockDevice:
			return "is a block device";
		case WriteStatus::ReadOnlyFilesystem:
			return "read-only filesystem";
		case WriteStatus::NotWritable:
			return "not writable";
		case WriteStatus::InvalidPath:
			return "invalid path";
		case WriteStatus::NoSpace:
			return "no space";
		case WriteStatus::TooLarge:
			return "file too large";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(RemoveStatus status) {
	switch (status) {
		case RemoveStatus::Ok:
			return "ok";
		case RemoveStatus::NotFound:
			return "not found";
		case RemoveStatus::IsDirectory:
			return "is a directory";
		case RemoveStatus::IsBlockDevice:
			return "is a block device";
		case RemoveStatus::DirectoryNotEmpty:
			return "directory not empty";
		case RemoveStatus::ReadOnlyFilesystem:
			return "read-only filesystem";
		case RemoveStatus::NotWritable:
			return "not writable";
		case RemoveStatus::InvalidPath:
			return "invalid path";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(RenameStatus status) {
	switch (status) {
		case RenameStatus::Ok:
			return "ok";
		case RenameStatus::SourceNotFound:
			return "source not found";
		case RenameStatus::TargetExists:
			return "target exists";
		case RenameStatus::IsBlockDevice:
			return "is a block device";
		case RenameStatus::ReadOnlyFilesystem:
			return "read-only filesystem";
		case RenameStatus::NotWritable:
			return "not writable";
		case RenameStatus::InvalidPath:
			return "invalid path";
		case RenameStatus::CrossFilesystem:
			return "cross-filesystem rename is not supported";
		case RenameStatus::CrossDirectory:
			return "cross-directory rename is not supported";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(SyncStatus status) {
	switch (status) {
		case SyncStatus::Ok:
			return "ok";
		case SyncStatus::Failed:
			return "sync failed";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(MountStatus status) {
	switch (status) {
		case MountStatus::Ok:
			return "ok";
		case MountStatus::InvalidSource:
			return "invalid source";
		case MountStatus::InvalidTarget:
			return "invalid target";
		case MountStatus::SourceNotFound:
			return "source not found";
		case MountStatus::SourceIsNotBlockDevice:
			return "source is not a block device";
		case MountStatus::TargetOutsideMountRoot:
			return "target must be /mnt/<name>";
		case MountStatus::TargetBusy:
			return "target busy";
		case MountStatus::NoMountSlot:
			return "no mount slot";
	}
	return "unknown error";
}

std::string_view lexxos::vfs::toString(UnmountStatus status) {
	switch (status) {
		case UnmountStatus::Ok:
			return "ok";
		case UnmountStatus::InvalidTarget:
			return "invalid target";
		case UnmountStatus::TargetNotMounted:
			return "target not mounted";
		case UnmountStatus::SyncFailed:
			return "sync failed";
	}
	return "unknown error";
}
