#include <lexxos/vfs/detail/vfs_mount_table.h>

#include <array>
#include <cstdint>
#include <cstring>

#include <lexxos/vfs/detail/vfs_string.h>
#include <lexxos/vfs/vfs.h>

namespace {

using lexxos::vfs::detail::FixedString;

constexpr std::string_view rootPath{"/"};

bool registerMountPath(lexxos::vfs::detail::BlockDeviceMount& mount, std::string_view name) {
	if (name.empty() || name.size() + 1 > mount.name.size()) {
		return false;
	}

	std::memcpy(mount.name.data(), name.data(), name.size());
	mount.name[name.size()] = '\0';

	FixedString path;
	path.append("/dev/");
	if (!path.append(name)) {
		return false;
	}

	return lexxos::vfs::detail::copyText(path.view(), mount.path.data(), mount.path.size());
}

bool hasNestedMountTargetSlash(std::string_view target) {
	constexpr std::string_view prefix{"/mnt/"};
	if (!lexxos::vfs::detail::startsWith(target, prefix) || target.size() <= prefix.size()) {
		return true;
	}

	for (std::size_t index = prefix.size(); index < target.size(); ++index) {
		if (target[index] == '/') {
			return true;
		}
	}
	return false;
}

lexxos::vfs::UnmountStatus unmountFilesystemImpl(
    std::array<lexxos::vfs::detail::FilesystemMount, lexxos::vfs::detail::maxFilesystemMounts>& filesystems,
    std::string_view target,
    bool force) {
	if (target.empty() || target.size() + 1 > lexxos::vfs::maxPathLength) {
		return lexxos::vfs::UnmountStatus::InvalidTarget;
	}
	for (auto& mount : filesystems) {
		if (mount.used && lexxos::vfs::detail::equals(mount.path.data(), target)) {
			auto status = lexxos::vfs::UnmountStatus::Ok;

			if (mount.filesystem != nullptr && !mount.filesystem->sync()) {
				status = lexxos::vfs::UnmountStatus::SyncFailed;
				if (!force) {
					return status;
				}
			}
			mount = {};
			return status;
		}
	}

	return lexxos::vfs::UnmountStatus::TargetNotMounted;
}

} // namespace

void lexxos::vfs::Vfs::initializeMountTable() {
	for (auto& mount : _blockDevices) {
		mount = {};
	}
	for (auto& mount : _filesystems) {
		mount = {};
	}
	for (auto& factory : _filesystemFactories) {
		factory = {};
	}
}

lexxos::vfs::detail::BlockDeviceMount* lexxos::vfs::Vfs::findBlockDeviceByPath(std::string_view path) {
	for (auto& mount : _blockDevices) {
		if (!mount.used) {
			continue;
		}
		if (detail::equals(path, mount.path.data()) || detail::equals(path, mount.name.data())) {
			return &mount;
		}
	}
	return nullptr;
}

lexxos::vfs::detail::FilesystemMount* lexxos::vfs::Vfs::findFilesystemByPath(std::string_view path) {
	detail::FilesystemMount* best = nullptr;
	std::size_t bestSize = 0;

	for (auto& mount : _filesystems) {
		if (!mount.used || mount.filesystem == nullptr) {
			continue;
		}

		std::string_view const mountPath{mount.path.data()};
		bool const exact = detail::equals(path, mountPath);
		bool const child = path.size() > mountPath.size() && detail::startsWith(path, mountPath) && path[mountPath.size()] == '/';
		if ((exact || child) && mountPath.size() > bestSize) {
			best = &mount;
			bestSize = mountPath.size();
		}
	}

	return best;
}

std::string_view lexxos::vfs::Vfs::pathInsideMount(detail::FilesystemMount const& mount, std::string_view path) {
	std::string_view const mountPath{mount.path.data()};
	if (detail::equals(path, mountPath)) {
		return rootPath;
	}
	return path.substr(mountPath.size());
}

bool lexxos::vfs::Vfs::listBlockDevices(DirectoryVisitor visitor, void* userData) {
	if (!detail::visitEntry(visitor, userData, "null", NodeType::RegularFile)) {
		return false;
	}
	for (auto const& mount : _blockDevices) {
		if (!mount.used) {
			continue;
		}

		std::uint64_t const size = mount.device != nullptr
		    ? mount.device->blockCount() * static_cast<std::uint64_t>(mount.device->blockSizeBytes())
		    : 0;
		if (!detail::visitEntry(visitor, userData, mount.name.data(), NodeType::BlockDevice, size)) {
			return false;
		}
	}
	return true;
}

bool lexxos::vfs::Vfs::listMountTargets(DirectoryVisitor visitor, void* userData) {
	for (auto const& mount : _filesystems) {
		if (!mount.used) {
			continue;
		}

		std::string_view const path{mount.path.data()};
		constexpr std::string_view prefix{"/mnt/"};
		if (!detail::startsWith(path, prefix)) {
			continue;
		}

		std::string_view const name = path.substr(prefix.size());
		if (!name.empty() && !detail::visitEntry(visitor, userData, name, NodeType::Directory)) {
			return false;
		}
	}
	return true;
}

bool lexxos::vfs::Vfs::registerBlockDevice(std::string_view name, lexxos::devices::BlockDevice* device) {
	if (device == nullptr) {
		return false;
	}

	for (auto& mount : _blockDevices) {
		if (mount.used && detail::equals(mount.name.data(), name)) {
			mount.device = device;
			return true;
		}
	}

	for (auto& mount : _blockDevices) {
		if (mount.used) {
			continue;
		}

		if (!registerMountPath(mount, name)) {
			return false;
		}
		mount.used = true;
		mount.device = device;
		return true;
	}

	return false;
}

lexxos::devices::BlockDevice* lexxos::vfs::Vfs::blockDevice(std::string_view path) {
	if (auto* mount = findBlockDeviceByPath(path); mount != nullptr) {
		return mount->device;
	}
	return nullptr;
}

lexxos::vfs::MountStatus lexxos::vfs::Vfs::mountFilesystem(
    std::string_view source,
    std::string_view target,
    ReadOnlyFilesystem* filesystem) {
	if (source.empty() || source.size() + 1 > maxPathLength) {
		return MountStatus::InvalidSource;
	}
	if (target.empty() || target.size() + 1 > maxPathLength) {
		return MountStatus::InvalidTarget;
	}
	if (filesystem == nullptr || !filesystem->isReady()) {
		return MountStatus::InvalidSource;
	}
	if (hasNestedMountTargetSlash(target)) {
		return MountStatus::TargetOutsideMountRoot;
	}
	if (blockDevice(source) == nullptr) {
		return MountStatus::SourceNotFound;
	}

	for (auto const& mount : _filesystems) {
		if (mount.used && detail::equals(mount.path.data(), target)) {
			return MountStatus::TargetBusy;
		}
	}

	for (auto& mount : _filesystems) {
		if (mount.used) {
			continue;
		}
		if (!detail::copyText(source, mount.source.data(), mount.source.size())) {
			return MountStatus::InvalidSource;
		}
		if (!detail::copyText(target, mount.path.data(), mount.path.size())) {
			return MountStatus::InvalidTarget;
		}
		mount.filesystem = filesystem;
		mount.used = true;
		return MountStatus::Ok;
	}

	return MountStatus::NoMountSlot;
}

lexxos::vfs::UnmountStatus lexxos::vfs::Vfs::unmountFilesystem(std::string_view target) {
	return unmountFilesystemImpl(_filesystems, target, false);
}

lexxos::vfs::UnmountStatus lexxos::vfs::Vfs::forceUnmountFilesystem(std::string_view target) {
	return unmountFilesystemImpl(_filesystems, target, true);
}

bool lexxos::vfs::Vfs::isMountTarget(std::string_view target) {
	for (auto const& mount : _filesystems) {
		if (mount.used && detail::equals(mount.path.data(), target)) {
			return true;
		}
	}

	return false;
}

lexxos::vfs::SyncStatus lexxos::vfs::Vfs::sync() {
	for (auto const& mount : _filesystems) {
		if (!mount.used || mount.filesystem == nullptr) {
			continue;
		}
		if (!mount.filesystem->sync()) {
			return SyncStatus::Failed;
		}
	}
	return SyncStatus::Ok;
}

bool lexxos::vfs::Vfs::visitMounts(MountVisitor visitor, void* userData) {
	if (!visitor({.source = "rootfs", .target = "/", .type = "virtualfs", .ready = true, .readOnly = false}, userData)) {
		return false;
	}

	for (auto const& mount : _blockDevices) {
		if (!mount.used) {
			continue;
		}

		bool const ready = mount.device != nullptr && mount.device->isReady();
		if (!visitor(
		        {.source = mount.name.data(),
		            .target = mount.path.data(),
		            .type = "block",
		            .ready = ready,
		            .readOnly = false},
		        userData)) {
			return false;
		}
	}

	for (auto const& mount : _filesystems) {
		if (!mount.used || mount.filesystem == nullptr) {
			continue;
		}
		if (!visitor(
		        {.source = mount.source.data(),
		            .target = mount.path.data(),
		            .type = mount.filesystem->typeName(),
		            .ready = mount.filesystem->isReady(),
		            .readOnly = mount.filesystem->isReadOnly()},
		        userData)) {
			return false;
		}
	}

	return true;
}

lexxos::vfs::VfsInfo lexxos::vfs::Vfs::vfsInfo() {
	VfsInfo info{
	    .maxBlockDevices = _blockDevices.size(),
	    .blockDevices = 0,
	    .maxFilesystemMounts = _filesystems.size(),
	    .filesystemMounts = 0,
	    .tmp = tmpInfo(),
	};

	for (auto const& mount : _blockDevices) {
		if (mount.used) {
			++info.blockDevices;
		}
	}
	for (auto const& mount : _filesystems) {
		if (mount.used) {
			++info.filesystemMounts;
		}
	}

	return info;
}

lexxos::vfs::detail::FilesystemFactoryEntry* lexxos::vfs::Vfs::findFilesystemFactory(std::string_view type) {
	for (auto& entry : _filesystemFactories) {
		if (entry.used && detail::equals(entry.type.data(), type)) {
			return &entry;
		}
	}
	return nullptr;
}

bool lexxos::vfs::Vfs::registerFilesystemFactory(
    std::string_view type,
    FilesystemFactory factory,
    void* userData) {
	if (type.empty() || type.size() + 1 > maxNameLength || factory == nullptr) {
		return false;
	}

	if (auto* existing = findFilesystemFactory(type); existing != nullptr) {
		existing->factory = factory;
		existing->userData = userData;
		return true;
	}

	for (auto& entry : _filesystemFactories) {
		if (entry.used) {
			continue;
		}
		if (!detail::copyText(type, entry.type.data(), entry.type.size())) {
			return false;
		}
		entry.factory = factory;
		entry.userData = userData;
		entry.used = true;
		return true;
	}

	return false;
}

lexxos::vfs::MountStatus lexxos::vfs::Vfs::mount(
    std::string_view type,
    std::string_view source,
    std::string_view target) {
	auto* entry = findFilesystemFactory(type);
	if (entry == nullptr || entry->factory == nullptr) {
		return MountStatus::InvalidSource;
	}
	return entry->factory(*this, source, target, entry->userData);
}

bool lexxos::vfs::registerBlockDevice(std::string_view name, lexxos::devices::BlockDevice* device) {
	return defaultVfs().registerBlockDevice(name, device);
}

lexxos::devices::BlockDevice* lexxos::vfs::blockDevice(std::string_view path) {
	return defaultVfs().blockDevice(path);
}

lexxos::vfs::MountStatus lexxos::vfs::mountFilesystem(
    std::string_view source,
    std::string_view target,
    ReadOnlyFilesystem* filesystem) {
	return defaultVfs().mountFilesystem(source, target, filesystem);
}

lexxos::vfs::UnmountStatus lexxos::vfs::unmountFilesystem(std::string_view target) {
	return defaultVfs().unmountFilesystem(target);
}

lexxos::vfs::UnmountStatus lexxos::vfs::forceUnmountFilesystem(std::string_view target) {
	return defaultVfs().forceUnmountFilesystem(target);
}

bool lexxos::vfs::isMountTarget(std::string_view target) {
	return defaultVfs().isMountTarget(target);
}

lexxos::vfs::SyncStatus lexxos::vfs::sync() {
	return defaultVfs().sync();
}

bool lexxos::vfs::visitMounts(MountVisitor visitor, void* userData) {
	return defaultVfs().visitMounts(visitor, userData);
}

lexxos::vfs::VfsInfo lexxos::vfs::vfsInfo() {
	return defaultVfs().vfsInfo();
}

bool lexxos::vfs::registerFilesystemFactory(
    std::string_view type,
    FilesystemFactory factory,
    void* userData) {
	return defaultVfs().registerFilesystemFactory(type, factory, userData);
}

lexxos::vfs::MountStatus lexxos::vfs::mount(
    std::string_view type,
    std::string_view source,
    std::string_view target) {
	return defaultVfs().mount(type, source, target);
}
