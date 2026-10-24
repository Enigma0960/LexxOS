#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::gameboy {

static constexpr std::size_t cartridgeHeaderSize = 0x150;
static constexpr std::size_t cartridgeLogoOffset = 0x104;
static constexpr std::size_t cartridgeLogoSize = 48;
static constexpr std::size_t cartridgeTitleOffset = 0x134;
static constexpr std::size_t cartridgeTitleSize = 16;
static constexpr std::size_t cartridgeCgbFlagOffset = 0x143;
static constexpr std::size_t cartridgeSgbFlagOffset = 0x146;
static constexpr std::size_t cartridgeTypeOffset = 0x147;
static constexpr std::size_t cartridgeRomSizeOffset = 0x148;
static constexpr std::size_t cartridgeRamSizeOffset = 0x149;
static constexpr std::size_t cartridgeDestinationOffset = 0x14A;
static constexpr std::size_t cartridgeOldLicenseeOffset = 0x14B;
static constexpr std::size_t cartridgeMaskRomVersionOffset = 0x14C;
static constexpr std::size_t cartridgeHeaderChecksumOffset = 0x14D;
static constexpr std::size_t cartridgeGlobalChecksumOffset = 0x14E;

struct CartridgeHeaderInfo {
	std::array<char, cartridgeTitleSize + 1> title{};
	std::uint8_t cgbFlag{0};
	std::uint8_t sgbFlag{0};
	std::uint8_t cartridgeType{0};
	std::uint8_t romSizeCode{0};
	std::uint8_t ramSizeCode{0};
	std::uint8_t destinationCode{0};
	std::uint8_t oldLicenseeCode{0};
	std::uint8_t maskRomVersion{0};
	std::uint8_t headerChecksum{0};
	std::uint16_t globalChecksum{0};
	std::uint8_t calculatedHeaderChecksum{0};
	bool nintendoLogoOk{false};
	bool headerChecksumOk{false};
};

[[nodiscard]] CartridgeHeaderInfo parseCartridgeHeader(std::uint8_t const* data, std::size_t size);
[[nodiscard]] bool hasCompleteCartridgeHeader(std::size_t size);

[[nodiscard]] std::string_view cartridgeTypeName(std::uint8_t code);
[[nodiscard]] std::string_view romSizeName(std::uint8_t code);
[[nodiscard]] std::uint32_t romSizeBytes(std::uint8_t code);
[[nodiscard]] std::string_view ramSizeName(std::uint8_t code);
[[nodiscard]] std::uint32_t ramSizeBytes(std::uint8_t code);
[[nodiscard]] std::string_view destinationName(std::uint8_t code);
[[nodiscard]] std::string_view cgbFlagName(std::uint8_t code);
[[nodiscard]] std::string_view sgbFlagName(std::uint8_t code);

} // namespace lexxos::gameboy
