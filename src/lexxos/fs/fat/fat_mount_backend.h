#pragma once
#include <lexxos/fs/fat/fat_mount_operations.h>

namespace lexxos::fs::fat {

// Доступ сервиса носителя к реестру FAT-томов.
class FatMountBackend {
public:
	virtual ~FatMountBackend();
	virtual bool isMounted(std::string_view target) = 0;
	virtual FatMountStatus mount(vfs::Vfs& vfs, std::string_view source, std::string_view target) = 0;
	virtual vfs::UnmountStatus unmount(vfs::Vfs& vfs, std::string_view target) = 0;
	virtual vfs::UnmountStatus forceUnmount(vfs::Vfs& vfs, std::string_view target) = 0;
};

} // namespace lexxos::fs::fat
