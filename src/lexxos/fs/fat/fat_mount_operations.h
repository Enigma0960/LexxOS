#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::fs::fat {

enum class FatMountStatus : std::uint8_t {
	Ok,
	NoDevice,
	DeviceNotReady,
	ReadError,
	UnsupportedSectorSize,
	UnsupportedFatType,
	InvalidBootSector,
	NoSupportedPartition,
	AlreadyMounted,
	NoMountSlot,
};

[[nodiscard]] char const* toString(FatMountStatus status);

[[nodiscard]] FatMountStatus mount(lexxos::vfs::Vfs& vfs, std::string_view source, std::string_view target);
[[nodiscard]] FatMountStatus mount(std::string_view source, std::string_view target);
lexxos::vfs::UnmountStatus unmount(lexxos::vfs::Vfs& vfs, std::string_view target);
lexxos::vfs::UnmountStatus unmount(std::string_view target);
lexxos::vfs::UnmountStatus forceUnmount(lexxos::vfs::Vfs& vfs, std::string_view target);
lexxos::vfs::UnmountStatus forceUnmount(std::string_view target);
[[nodiscard]] bool isMounted(std::string_view target);

} // namespace lexxos::fs::fat
