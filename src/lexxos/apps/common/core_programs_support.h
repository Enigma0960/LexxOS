#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/board.h>
#include <lexxos/program/arguments.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps {

inline constexpr std::uint8_t storageProbeAttempts = 3;
inline constexpr std::uint8_t mountProbeAttempts = 4;
inline constexpr std::size_t storageBlockSizeBytes = 512;
using StorageBlock = std::array<std::uint8_t, storageBlockSizeBytes>;

template <std::size_t Size>
constexpr lexxos::program::ProgramArgumentList argumentList(lexxos::program::ProgramArgumentSpec const (&arguments)[Size]) {
	return lexxos::program::ProgramArgumentList{
	    .data = arguments,
	    .size = Size,
	};
}

[[nodiscard]] bool parseOptionalUnsignedArgument(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view name,
    std::uint32_t& value);
void writeUnsigned(lexxos::os::AppContext& context, std::uint32_t value);
void writeUnsigned64(lexxos::os::AppContext& context, std::uint64_t value);
void writeHex8(lexxos::os::AppContext& context, std::uint8_t value);
void writeHexByte(lexxos::os::AppContext& context, std::uint8_t value);
void writeHex32(lexxos::os::AppContext& context, std::uint32_t value);
[[nodiscard]] bool parseUnsigned32(std::string_view text, std::uint32_t& value);
[[nodiscard]] bool parseUnsigned64(std::string_view text, std::uint64_t& value);
[[nodiscard]] std::uint32_t uptimeMs(lexxos::os::AppContext const& context);
void sleepMs(lexxos::os::AppContext const& context, std::uint32_t milliseconds);
[[nodiscard]] lexxos::platform::BoardDescriptor const& activeBoard(lexxos::os::AppContext const& context);

[[nodiscard]] bool isPrintableAscii(std::uint8_t value);
[[nodiscard]] std::uint16_t readLe16(std::uint8_t const* data);
[[nodiscard]] std::uint32_t readLe32(std::uint8_t const* data);

[[nodiscard]] lexxos::devices::BlockDevice* sdBlockDevice(lexxos::os::AppContext& context);
[[nodiscard]] lexxos::devices::DeviceResourceLock lockSdBlockDevice(lexxos::os::AppContext& context, std::string_view programName);

void writeBlockDeviceSummary(lexxos::os::AppContext& context, lexxos::devices::BlockDevice const* device);
void writeStorageSummary(lexxos::os::AppContext& context);
void writeTmpSummary(lexxos::os::AppContext& context, lexxos::vfs::TmpInfo const& info);

[[nodiscard]] bool probeStorage(lexxos::os::AppContext& context, std::uint8_t attempts);
[[nodiscard]] bool ensureStorageReady(lexxos::os::AppContext& context);
[[nodiscard]] bool readRawStorageBlock0(
    lexxos::os::AppContext& context,
    StorageBlock& sector);
[[nodiscard]] bool isSdBlockSource(std::string_view source);
[[nodiscard]] bool prepareSdCardForMount(lexxos::os::AppContext& context);
[[nodiscard]] bool readStorageBlock0(
    lexxos::os::AppContext& context,
    StorageBlock& sector);
void printStorageBlock0Summary(
    lexxos::os::AppContext& context,
    StorageBlock const& sector);

[[nodiscard]] bool resolvePath(
    lexxos::os::AppContext& context,
    std::string_view path,
    std::array<char, lexxos::vfs::maxPathLength>& output);
void writePinValue(lexxos::os::AppContext& context, lexxos::platform::GpioPin pin);
void writePin(lexxos::os::AppContext& context, lexxos::platform::GpioPin pin);

} // namespace lexxos::apps
