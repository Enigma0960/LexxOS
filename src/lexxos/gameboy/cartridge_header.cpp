#include <lexxos/gameboy/cartridge_header.h>

namespace lexxos::gameboy {
namespace {

	// @formatter:off
	constexpr std::uint8_t expectedNintendoLogo[cartridgeLogoSize] = {
	    0xCE,
	    0xED,
	    0x66,
	    0x66,
	    0xCC,
	    0x0D,
	    0x00,
	    0x0B,
	    0x03,
	    0x73,
	    0x00,
	    0x83,
	    0x00,
	    0x0C,
	    0x00,
	    0x0D,
	    0x00,
	    0x08,
	    0x11,
	    0x1F,
	    0x88,
	    0x89,
	    0x00,
	    0x0E,
	    0xDC,
	    0xCC,
	    0x6E,
	    0xE6,
	    0xDD,
	    0xDD,
	    0xD9,
	    0x99,
	    0xBB,
	    0xBB,
	    0x67,
	    0x63,
	    0x6E,
	    0x0E,
	    0xEC,
	    0xCC,
	    0xDD,
	    0xDC,
	    0x99,
	    0x9F,
	    0xBB,
	    0xB9,
	    0x33,
	    0x3E,
	};

	// @formatter:on

	bool isTitleCharacter(std::uint8_t value) {
		return value >= 0x20U && value <= 0x7EU;
	}

	std::uint16_t readBe16(std::uint8_t const* data) {
		return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
	}

	std::uint8_t calculateHeaderChecksum(std::uint8_t const* data) {
		std::uint8_t checksum = 0;
		for (std::size_t index = cartridgeTitleOffset; index <= cartridgeMaskRomVersionOffset; ++index) {
			checksum = static_cast<std::uint8_t>(checksum - data[index] - 1U);
		}
		return checksum;
	}

	bool checkNintendoLogo(std::uint8_t const* data) {
		for (std::size_t index = 0; index < cartridgeLogoSize; ++index) {
			if (data[cartridgeLogoOffset + index] != expectedNintendoLogo[index]) {
				return false;
			}
		}
		return true;
	}

	void parseTitle(std::uint8_t const* data, std::array<char, cartridgeTitleSize + 1>& output) {
		std::size_t outputIndex = 0;
		for (std::size_t index = 0; index < cartridgeTitleSize && outputIndex + 1 < output.size(); ++index) {
			auto const value = data[cartridgeTitleOffset + index];
			if (value == 0) {
				break;
			}
			if (!isTitleCharacter(value)) {
				break;
			}
			output[outputIndex] = static_cast<char>(value);
			++outputIndex;
		}
		output[outputIndex] = '\0';
	}

} // namespace

bool hasCompleteCartridgeHeader(std::size_t size) {
	return size >= cartridgeHeaderSize;
}

CartridgeHeaderInfo parseCartridgeHeader(std::uint8_t const* data, std::size_t size) {
	CartridgeHeaderInfo info{};
	if (data == nullptr || !hasCompleteCartridgeHeader(size)) {
		return info;
	}

	parseTitle(data, info.title);
	info.cgbFlag = data[cartridgeCgbFlagOffset];
	info.sgbFlag = data[cartridgeSgbFlagOffset];
	info.cartridgeType = data[cartridgeTypeOffset];
	info.romSizeCode = data[cartridgeRomSizeOffset];
	info.ramSizeCode = data[cartridgeRamSizeOffset];
	info.destinationCode = data[cartridgeDestinationOffset];
	info.oldLicenseeCode = data[cartridgeOldLicenseeOffset];
	info.maskRomVersion = data[cartridgeMaskRomVersionOffset];
	info.headerChecksum = data[cartridgeHeaderChecksumOffset];
	info.globalChecksum = readBe16(&data[cartridgeGlobalChecksumOffset]);
	info.calculatedHeaderChecksum = calculateHeaderChecksum(data);
	info.headerChecksumOk = info.headerChecksum == info.calculatedHeaderChecksum;
	info.nintendoLogoOk = checkNintendoLogo(data);
	return info;
}

std::string_view cartridgeTypeName(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return "ROM ONLY";
		case 0x01:
			return "MBC1";
		case 0x02:
			return "MBC1+RAM";
		case 0x03:
			return "MBC1+RAM+BATTERY";
		case 0x05:
			return "MBC2";
		case 0x06:
			return "MBC2+BATTERY";
		case 0x08:
			return "ROM+RAM";
		case 0x09:
			return "ROM+RAM+BATTERY";
		case 0x0B:
			return "MMM01";
		case 0x0C:
			return "MMM01+RAM";
		case 0x0D:
			return "MMM01+RAM+BATTERY";
		case 0x0F:
			return "MBC3+TIMER+BATTERY";
		case 0x10:
			return "MBC3+TIMER+RAM+BATTERY";
		case 0x11:
			return "MBC3";
		case 0x12:
			return "MBC3+RAM";
		case 0x13:
			return "MBC3+RAM+BATTERY";
		case 0x19:
			return "MBC5";
		case 0x1A:
			return "MBC5+RAM";
		case 0x1B:
			return "MBC5+RAM+BATTERY";
		case 0x1C:
			return "MBC5+RUMBLE";
		case 0x1D:
			return "MBC5+RUMBLE+RAM";
		case 0x1E:
			return "MBC5+RUMBLE+RAM+BATTERY";
		case 0x20:
			return "MBC6";
		case 0x22:
			return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
		case 0xFC:
			return "POCKET CAMERA";
		case 0xFD:
			return "BANDAI TAMA5";
		case 0xFE:
			return "HuC3";
		case 0xFF:
			return "HuC1+RAM+BATTERY";
	}
	return "unknown";
}

std::uint32_t romSizeBytes(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return 32U * 1024U;
		case 0x01:
			return 64U * 1024U;
		case 0x02:
			return 128U * 1024U;
		case 0x03:
			return 256U * 1024U;
		case 0x04:
			return 512U * 1024U;
		case 0x05:
			return 1024U * 1024U;
		case 0x06:
			return 2U * 1024U * 1024U;
		case 0x07:
			return 4U * 1024U * 1024U;
		case 0x08:
			return 8U * 1024U * 1024U;
		case 0x52:
			return 1152U * 1024U;
		case 0x53:
			return 1280U * 1024U;
		case 0x54:
			return 1536U * 1024U;
	}
	return 0;
}

std::string_view romSizeName(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return "32 KiB";
		case 0x01:
			return "64 KiB";
		case 0x02:
			return "128 KiB";
		case 0x03:
			return "256 KiB";
		case 0x04:
			return "512 KiB";
		case 0x05:
			return "1 MiB";
		case 0x06:
			return "2 MiB";
		case 0x07:
			return "4 MiB";
		case 0x08:
			return "8 MiB";
		case 0x52:
			return "1.1 MiB";
		case 0x53:
			return "1.2 MiB";
		case 0x54:
			return "1.5 MiB";
	}
	return "unknown";
}

std::uint32_t ramSizeBytes(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return 0;
		case 0x01:
			return 2U * 1024U;
		case 0x02:
			return 8U * 1024U;
		case 0x03:
			return 32U * 1024U;
		case 0x04:
			return 128U * 1024U;
		case 0x05:
			return 64U * 1024U;
	}
	return 0;
}

std::string_view ramSizeName(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return "none";
		case 0x01:
			return "2 KiB";
		case 0x02:
			return "8 KiB";
		case 0x03:
			return "32 KiB";
		case 0x04:
			return "128 KiB";
		case 0x05:
			return "64 KiB";
	}
	return "unknown";
}

std::string_view destinationName(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return "Japan";
		case 0x01:
			return "Overseas";
	}
	return "unknown";
}

std::string_view cgbFlagName(std::uint8_t code) {
	switch (code) {
		case 0x00:
			return "no";
		case 0x80:
			return "supported";
		case 0xC0:
			return "required";
	}
	return "unknown";
}

std::string_view sgbFlagName(std::uint8_t code) {
	return code == 0x03 ? "supported" : "no";
}

} // namespace lexxos::gameboy
