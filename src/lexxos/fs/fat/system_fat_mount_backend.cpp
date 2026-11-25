#include <lexxos/fs/fat/system_fat_mount_backend.h>

namespace lexxos::fs::fat {

bool SystemFatMountBackend::isMounted(std::string_view target) {
	return lexxos::fs::fat::isMounted(target);
}

FatMountStatus SystemFatMountBackend::mount(vfs::Vfs& vfs, std::string_view source, std::string_view target) {
	return lexxos::fs::fat::mount(vfs, source, target);
}

vfs::UnmountStatus SystemFatMountBackend::unmount(vfs::Vfs& vfs, std::string_view target) {
	return lexxos::fs::fat::unmount(vfs, target);
}

vfs::UnmountStatus SystemFatMountBackend::forceUnmount(vfs::Vfs& vfs, std::string_view target) {
	return lexxos::fs::fat::forceUnmount(vfs, target);
}

} // namespace lexxos::fs::fat
