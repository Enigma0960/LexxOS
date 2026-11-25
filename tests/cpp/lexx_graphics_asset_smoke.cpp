#include <cassert>
#include <cstdint>

#include <lexxos/devices/display/rgb565_blend.h>
#include <lexxos/external_program/lexx_graphics_asset.h>
#include <lexxos/graphics/ascii_font_5x7.h>

int main() {
	using lexxos::external_program::graphics_asset::crcMatches;
	using lexxos::external_program::graphics_asset::decodeHeader;
	using lexxos::external_program::graphics_asset::glyphColumn;
	using lexxos::external_program::graphics_asset::headerBytes;
	using lexxos::external_program::graphics_asset::Info;
	std::uint8_t const image[] = {
	    'L',
	    'X',
	    'I',
	    'M',
	    2,
	    0,
	    1,
	    0,
	    1,
	    0,
	    0,
	    0,
	    0x1C,
	    0xDF,
	    0x44,
	    0x21,
	    0,
	    0,
	    0,
	    0,
	};
	Info info{};
	// CRC of four zero bytes.
	assert(decodeHeader(image, sizeof(image), info));
	assert(info.payloadBytes == 4 && !info.hasAlpha && info.crc32 == 0x2144DF1CU);
	assert(info.width == 2 && info.height == 1);
	assert(crcMatches(image + headerBytes, 4, info.crc32));
	std::uint8_t wrong[] = {'L', 'X', 'I', 'M', 2, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0};
	assert(!decodeHeader(wrong, sizeof(wrong), info));
	std::uint8_t transparent[] = {
	    'L',
	    'X',
	    'I',
	    'M',
	    2,
	    0,
	    1,
	    0,
	    1,
	    0,
	    1,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    128,
	};
	assert(decodeHeader(transparent, sizeof(transparent), info));
	assert(info.hasAlpha && info.payloadBytes == 6 && info.width == 2);
	transparent[10] = 2; // Unknown flag rejected.
	assert(!decodeHeader(transparent, sizeof(transparent), info));
	assert(glyphColumn('F', 0) != 0 && glyphColumn('0', 0) != 0);
	// Every printable ASCII character has a glyph (except the blank space).
	for (int character = 0x20; character <= 0x7E; ++character) {
		std::uint8_t pixels = 0;
		for (int column = 0; column < 5; ++column) {
			pixels |= glyphColumn(static_cast<char>(character), column);
		}

		assert((pixels != 0U) == (character != ' '));
	}

	// Uppercase and lowercase characters are separate glyphs.
	bool different = false;
	for (int column = 0; column < 5; ++column) {
		different |= glyphColumn('A', column) != glyphColumn('a', column);
	}

	assert(different);
	for (int column = 0; column < 5; ++column) {
		assert(glyphColumn(static_cast<char>(0x7F), column) == glyphColumn('?', column));
		assert(glyphColumn(static_cast<char>(0xFF), column) == glyphColumn('?', column));
	}

	assert(glyphColumn('A', -1) == 0U && glyphColumn('A', 5) == 0U);
	assert(lexxos::graphics::ascii_font::glyphRow('A', -1) == 0U);
	assert(lexxos::graphics::ascii_font::glyphRow('A', 7) == 0U);
	assert(lexxos::devices::blendRgb565(0xF800, 0x001F, 128) == 0x800F);
	// CRC may be calculated over bounded pieces without allocating the image.
	std::uint32_t incremental = 0xFFFFFFFFU;
	incremental = lexxos::external_program::graphics_asset::crc32Update(incremental, image + 16, 2);
	incremental = lexxos::external_program::graphics_asset::crc32Update(incremental, image + 18, 2);
	assert((~incremental) == lexxos::external_program::graphics_asset::crc32(image + 16, 4));
	std::uint8_t largeHeader[16]{'L', 'X', 'I', 'M', 0, 2, 0, 2, 1};
	assert(!decodeHeader(largeHeader, 16U + 512U * 512U * 2U, info));
	assert(decodeHeader(largeHeader, 16U + 512U * 512U * 2U, info, 262144U, 512U));
	assert(info.width == 512U && info.height == 512U);
}
