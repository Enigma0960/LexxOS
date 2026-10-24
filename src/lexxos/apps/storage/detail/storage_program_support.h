#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

inline constexpr std::uint32_t storageBlockSize = 512;
inline constexpr std::uint32_t sdBenchDefaultBlocks = 128;
inline constexpr std::uint32_t sdBenchMaxBlocks = 1024;
inline constexpr std::uint32_t sdStressDefaultPasses = 3;
inline constexpr std::uint32_t sdStressMaxPasses = 20;
inline constexpr std::uint32_t sdStressDefaultFiles = 4;
inline constexpr std::uint32_t sdStressMaxFiles = 8;
inline constexpr std::uint32_t sdSpeedDefaultBlocksPerFile = 64;
inline constexpr std::uint32_t sdSpeedMaxBlocksPerFile = 1024;
inline constexpr std::uint32_t sdSpeedDefaultFiles = 8;
inline constexpr std::uint32_t sdSpeedMaxFiles = 64;
inline constexpr std::uint32_t sdSpeedDefaultRawBatchBlocks = 2;
inline constexpr std::uint32_t sdSpeedMaxRawBatchBlocks = 2;
inline constexpr lexxos::devices::SpiClockMode sdSpeedReferenceMode = lexxos::devices::SpiClockMode::Mode0;

inline constexpr std::string_view sdTestDefaultPath = "/mnt/sd/LEXTEST.TXT";
inline constexpr std::string_view sdBenchDefaultPath = "/mnt/sd/LEXBENCH.TXT";
inline constexpr std::string_view sdTestPayload = "LexxOS STM32F407 SDIO FAT read/write test\n";

[[nodiscard]] std::uint32_t sdSpeedReferenceBaudRate();
[[nodiscard]] bool parseOptionalCount(lexxos::os::AppContext& context, std::string_view text,
    std::uint32_t fallback, std::uint32_t maximum, char const* name, std::uint32_t& output);
[[nodiscard]] std::uint32_t elapsedMs(std::uint32_t startMs, std::uint32_t endMs);
void writeThroughput(lexxos::os::AppContext& context, std::uint64_t bytes, std::uint32_t milliseconds);
void fillPattern(std::array<char, storageBlockSize>& buffer, std::uint32_t seed, std::uint64_t offset);
[[nodiscard]] bool verifyPattern(std::array<char, storageBlockSize> const& buffer, std::size_t size,
    std::uint32_t seed, std::uint64_t offset);
[[nodiscard]] bool makeStressPath(std::uint32_t pass, std::uint32_t file,
    std::array<char, lexxos::vfs::maxPathLength>& path);
[[nodiscard]] bool makeSpeedPath(std::uint32_t file, std::array<char, lexxos::vfs::maxPathLength>& path);
[[nodiscard]] std::uint32_t hashBytes(std::uint32_t hash, void const* data, std::size_t size);
void cleanupSpeedFiles(std::uint32_t files);

} // namespace lexxos::apps::storage_programs::detail
