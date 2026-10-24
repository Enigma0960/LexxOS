#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/board.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::picocalc_programs::detail {

inline constexpr std::uint32_t psramDefaultTestAddress = 0;
inline constexpr std::uint32_t psramDefaultTestLength = 4096;
inline constexpr std::uint32_t psramMaxTestLength = 4096;
inline constexpr std::uint32_t psramAddressSpaceBytes = 0x01000000U;

using PsramTestBuffer = std::array<std::uint8_t, psramMaxTestLength>;

[[nodiscard]] bool parseOptionalUnsignedFlexible(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view name,
    std::uint32_t& value);
void writeHexAddress(lexxos::os::AppContext& context, std::uint32_t value);

[[nodiscard]] bool readPsramJedecId(
    lexxos::platform::BoardDescriptor const& board,
    std::array<std::uint8_t, 8>& output);
[[nodiscard]] bool readPsramMemory(
    lexxos::platform::PsramPins const& pins,
    std::uint32_t address,
    std::uint8_t* output,
    std::size_t length);
[[nodiscard]] bool writePsramMemory(
    lexxos::platform::PsramPins const& pins,
    std::uint32_t address,
    std::uint8_t const* data,
    std::size_t length);
void fillPsramPattern(
    PsramTestBuffer& buffer,
    std::uint32_t address,
    std::size_t length,
    std::uint8_t seed);
[[nodiscard]] bool verifyPsramBuffer(
    PsramTestBuffer const& expected,
    PsramTestBuffer const& actual,
    std::size_t length,
    std::size_t& mismatchOffset);
[[nodiscard]] bool idLooksIdle(std::array<std::uint8_t, 8> const& id, std::uint8_t value);

} // namespace lexxos::apps::picocalc_programs::detail
