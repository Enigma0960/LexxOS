#pragma once
#include <lexxos/fs/fat/fat_mount_backend.h>

namespace lexxos::fs::fat {

// Адаптер действующего общего реестра FatFs.
class SystemFatMountBackend final : public FatMountBackend {
public:
	bool isMounted(std::string_view target) override;
	FatMountStatus mount(vfs::Vfs& vfs, std::string_view source, std::string_view target) override;
	vfs::UnmountStatus unmount(vfs::Vfs& vfs, std::string_view target) override;
	vfs::UnmountStatus forceUnmount(vfs::Vfs& vfs, std::string_view target) override;
};

} // namespace lexxos::fs::fat
