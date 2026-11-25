#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/graphics/ascii_font_5x7.h>

namespace lexxos::external_program::graphics_asset {

// LXIM v1: magic[4], u16 width/height, u16 format=1 (RGB565LE),
// u16 flags: bit0=planar alpha8. CRC32 covers RGB565 plus optional alpha plane.
// Legacy opaque files (flags=0) remain binary compatible.
inline constexpr std::size_t headerBytes = 16;
inline constexpr std::uint32_t maxPixels = 8192;
inline constexpr std::uint16_t alpha8Flag = 1U;

struct Info {
	std::uint16_t width{0};
	std::uint16_t height{0};
	std::uint32_t crc32{0};
	std::uint32_t payloadBytes{0};
	bool hasAlpha{false};
};

inline std::uint16_t read16(std::uint8_t const* data) {
	return static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8U);
}

inline std::uint32_t read32(std::uint8_t const* data) {
	return static_cast<std::uint32_t>(data[0])
	    | (static_cast<std::uint32_t>(data[1]) << 8U)
	    | (static_cast<std::uint32_t>(data[2]) << 16U)
	    | (static_cast<std::uint32_t>(data[3]) << 24U);
}

inline bool decodeHeader(std::uint8_t const* bytes, std::uint32_t resourceBytes, Info& info,
    std::uint32_t allowedPixels = maxPixels, std::uint32_t allowedDimension = 320U) {
	info = {};
	if (bytes[0] != 'L' || bytes[1] != 'X' || bytes[2] != 'I' || bytes[3] != 'M' || read16(bytes + 8) != 1U) {
		return false;
	}

	auto const flags = read16(bytes + 10);
	if ((flags & ~alpha8Flag) != 0U) {
		return false;
	}

	auto const w = read16(bytes + 4);
	auto const h = read16(bytes + 6);

	std::uint32_t const pixels = static_cast<std::uint32_t>(w) * h;
	if (w == 0 || h == 0 || w > allowedDimension || h > allowedDimension || pixels > allowedPixels) {
		return false;
	}

	std::uint32_t const payload = pixels * ((flags & alpha8Flag) != 0U ? 3U : 2U);
	if (resourceBytes != headerBytes + payload) {
		return false;
	}

	info = {w, h, read32(bytes + 12), payload, (flags & alpha8Flag) != 0U};
	return true;
}

inline std::uint32_t crc32Update(std::uint32_t crc, std::uint8_t const* bytes, std::size_t size) {
	for (std::size_t i = 0; i < size; ++i) {
		crc ^= bytes[i];
		for (unsigned bit = 0; bit < 8U; ++bit) {
			crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
		}
	}

	return crc;
}

inline std::uint32_t crc32(std::uint8_t const* bytes, std::size_t size) {
	return ~crc32Update(0xFFFFFFFFU, bytes, size);
}

inline bool crcMatches(std::uint8_t const* bytes, std::size_t size, std::uint32_t expected) {
	return crc32(bytes, size) == expected;
}

// A minimal 5x7 ASCII HUD font, columns with top pixel in bit 0.
inline std::uint8_t glyphColumn(char c, int column) {
	if (column < 0 || column >= 5) {
		return 0;
	}

	char const upper = c;
	switch (upper) {
		case '0': {
			constexpr std::uint8_t bits[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
			return bits[column];
		}
		case '1': {
			constexpr std::uint8_t bits[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
			return bits[column];
		}
		case '2': {
			constexpr std::uint8_t bits[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
			return bits[column];
		}
		case '3': {
			constexpr std::uint8_t bits[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
			return bits[column];
		}
		case '4': {
			constexpr std::uint8_t bits[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
			return bits[column];
		}
		case '5': {
			constexpr std::uint8_t bits[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
			return bits[column];
		}
		case '6': {
			constexpr std::uint8_t bits[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
			return bits[column];
		}
		case '7': {
			constexpr std::uint8_t bits[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
			return bits[column];
		}
		case '8': {
			constexpr std::uint8_t bits[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
			return bits[column];
		}
		case '9': {
			constexpr std::uint8_t bits[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
			return bits[column];
		}
		case 'F': {
			constexpr std::uint8_t bits[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
			return bits[column];
		}
		case 'P': {
			constexpr std::uint8_t bits[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
			return bits[column];
		}
		case 'S': {
			constexpr std::uint8_t bits[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
			return bits[column];
		}
		case 'T': {
			constexpr std::uint8_t bits[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
			return bits[column];
		}
		case 'I': {
			constexpr std::uint8_t bits[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
			return bits[column];
		}
		case 'M': {
			constexpr std::uint8_t bits[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
			return bits[column];
		}
		case 'E': {
			constexpr std::uint8_t bits[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
			return bits[column];
		}
		case 'L': {
			constexpr std::uint8_t bits[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
			return bits[column];
		}
		case 'X': {
			constexpr std::uint8_t bits[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
			return bits[column];
		}
		case 'O': {
			constexpr std::uint8_t bits[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
			return bits[column];
		}
		case ':': {
			constexpr std::uint8_t bits[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
			return bits[column];
		}
		case '.': {
			constexpr std::uint8_t bits[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
			return bits[column];
		}
		case '-': {
			constexpr std::uint8_t bits[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
			return bits[column];
		}
		default: return lexxos::graphics::ascii_font::glyphColumn(static_cast<std::uint8_t>(c), column);
	}
}

} // namespace lexxos::external_program::graphics_asset
