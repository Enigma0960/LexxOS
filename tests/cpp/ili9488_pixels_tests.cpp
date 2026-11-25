#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#include <lexxos/drivers/core/display/ili9488_pixels.h>
#include <lexxos/drivers/core/display/ili9488_stream.h>

namespace {

namespace pixels = lexxos::drivers::core;

void check(bool ok) {
	if (!ok) {
		std::cerr << "ILI9488 pixel encoding failed\n";
		std::abort();
	}
}

struct Sink {
	std::vector<std::uint8_t> bytes;
	std::vector<std::size_t> sizes;
	unsigned failAt{};
};

bool send(void* context, std::uint8_t const* data, std::size_t size) {
	auto& sink = *static_cast<Sink*>(context);
	sink.sizes.push_back(size);
	if (sink.failAt == sink.sizes.size()) {
		return false;
	}

	sink.bytes.insert(sink.bytes.end(), data, data + size);
	return true;
}

void streaming() {
	std::uint16_t words[]{0xF800, 0x07E0, 0x001F, 0, 0xFFFF};
	std::uint8_t packed[]{0xF8, 0, 7, 0xE0, 0, 0x1F, 0, 0, 255, 255};
	std::uint8_t rgb[]{255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255};
	for (auto wire : {pixels::Ili9488PixelFormat::Rgb565, pixels::Ili9488PixelFormat::Rgb666}) {
		auto bpp = wire == pixels::Ili9488PixelFormat::Rgb565 ? 2U : 3U;
		std::vector<std::uint8_t> expected = bpp == 2 ? std::vector<std::uint8_t>(packed, packed + 10) : std::vector<std::uint8_t>(rgb, rgb + 15);
		for (auto source : {pixels::Ili9488PixelSource::Rgb565Words, pixels::Ili9488PixelSource::Rgb565Bytes, pixels::Ili9488PixelSource::Rgb888Bytes}) {
			void const* input = source == pixels::Ili9488PixelSource::Rgb565Words ? static_cast<void const*>(words) : source == pixels::Ili9488PixelSource::Rgb565Bytes ? packed
			                                                                                                                                                            : rgb;
			bool direct = (source == pixels::Ili9488PixelSource::Rgb565Bytes && bpp == 2) || (source == pixels::Ili9488PixelSource::Rgb888Bytes && bpp == 3);
			std::array<std::uint8_t, 8> buffer{};
			buffer.front() = 0xAB;
			buffer.back() = 0xCD;
			std::span<std::uint8_t> scratch{buffer.data() + 1, 6};
			Sink sink;
			pixels::Ili9488PixelTransport transport{&sink, send};
			check(pixels::streamIli9488Pixels(transport, wire, source, input, 5, scratch, 2));
			check(sink.bytes == expected);
			check(sink.sizes == (direct ? std::vector<std::size_t>{5 * bpp} : std::vector<std::size_t>{2 * bpp, 2 * bpp, bpp}));
			check(buffer.front() == 0xAB && buffer.back() == 0xCD);
			for (unsigned fail = 1; fail <= (direct ? 1U : 3U); ++fail) {
				sink = {.failAt = fail};
				check(!pixels::streamIli9488Pixels(transport, wire, source, input, 5, scratch, 2));
				check(sink.sizes.size() == fail);
			}

			sink = {};
			check(pixels::streamIli9488Pixels(transport, wire, source, nullptr, 0, {}, 0));
			check(!pixels::streamIli9488Pixels(transport, wire, source, nullptr, 1, scratch, 2));
			check(!pixels::streamIli9488Pixels(transport, wire, source, input, std::numeric_limits<std::size_t>::max(), scratch, 2));
			check(sink.sizes.empty());
			check(pixels::streamIli9488Pixels(transport, wire, source, input, 5, {}, 0) == direct);
		}
		for (std::uint16_t color : {0U, 0xFFFFU, 0xA15BU}) {
			std::array<std::uint8_t, 6> scratch{};
			Sink sink;
			pixels::Ili9488PixelTransport transport{&sink, send};
			check(pixels::fillIli9488Pixels(transport, wire, color, 5, scratch, 2));
			check(sink.sizes == std::vector<std::size_t>({2 * bpp, 2 * bpp, bpp}));
			std::vector<std::uint8_t> expectedFill(5 * bpp);
			if (bpp == 2) {
				pixels::fillRepeatedRgb565WirePixel(expectedFill.data(), color, 5);
			} else {
				pixels::fillRepeatedRgb888Pixel(expectedFill.data(), color, 5);
			}

			check(sink.bytes == expectedFill);
			for (unsigned fail = 1; fail <= 3; ++fail) {
				sink = {.failAt = fail};
				check(!pixels::fillIli9488Pixels(transport, wire, color, 5, scratch, 2));
				check(sink.sizes.size() == fail);
			}

			sink = {};
			check(!pixels::fillIli9488Pixels(transport, wire, color, 5, scratch, 4));
			check(sink.sizes.empty());
		}
	}
}

void exhaustiveColors() {
	for (unsigned value = 0; value <= 65535; ++value) {
		auto pixel = static_cast<std::uint16_t>(value);
		std::array<std::uint8_t, 5> rgb{0xAB, 0, 0, 0, 0xCD};
		std::array<std::uint8_t, 4> wire{0xAB, 0, 0, 0xCD};
		pixels::fillRgb888FromRgb565Pixels(rgb.data() + 1, &pixel, 1);
		unsigned red = value >> 11U, green = (value >> 5U) & 63U, blue = value & 31U;
		check(rgb[1] == red * 8U + red / 4U);
		check(rgb[2] == green * 4U + green / 16U);
		check(rgb[3] == blue * 8U + blue / 4U);
		check(rgb.front() == 0xAB && rgb.back() == 0xCD);
		pixels::fillRgb565WireFromRgb565Pixels(wire.data() + 1, &pixel, 1);
		check(wire[1] == value / 256U && wire[2] == value % 256U);
		check(wire.front() == 0xAB && wire.back() == 0xCD);
		auto expected = wire;
		pixels::fillRgb565WireFromRgb888Bytes(wire.data() + 1, rgb.data() + 1, 1);
		check(wire == expected);
		std::array<std::uint8_t, 3> converted{};
		pixels::fillRgb888FromRgb565Bytes(converted.data(), wire.data() + 1, 1);
		check(converted[0] == rgb[1] && converted[1] == rgb[2] && converted[2] == rgb[3]);
		std::uint8_t repeated = 0xAC;
		bool equal = rgb[1] == rgb[2] && rgb[2] == rgb[3];
		check(pixels::rgb565ExpandsToRepeatedRgb888Byte(pixel, repeated) == equal);
		check(repeated == (equal ? rgb[1] : 0xAC));
	}
}

void bursts() {
	for (std::size_t count : {0U, 1U, 511U, 512U, 513U, 4096U, 16385U}) {
		std::vector<std::uint16_t> input(count, 0xA15B);
		std::vector<std::uint8_t> wire(count * 2 + 2, 0xAD), rgb(count * 3 + 2, 0xAD);
		auto repeatedWire = wire, repeatedRgb = rgb;
		pixels::fillRgb565WireFromRgb565Pixels(wire.data() + 1, input.data(), count);
		pixels::fillRgb888FromRgb565Pixels(rgb.data() + 1, input.data(), count);
		pixels::fillRepeatedRgb565WirePixel(repeatedWire.data() + 1, 0xA15B, count);
		pixels::fillRepeatedRgb888Pixel(repeatedRgb.data() + 1, 0xA15B, count);
		check(wire == repeatedWire && rgb == repeatedRgb);
		check(wire.front() == 0xAD && wire.back() == 0xAD && rgb.front() == 0xAD && rgb.back() == 0xAD);
	}
	pixels::fillRgb888FromRgb565Pixels(nullptr, nullptr, 0);
	pixels::fillRgb888FromRgb565Bytes(nullptr, nullptr, 0);
	pixels::fillRgb565WireFromRgb565Pixels(nullptr, nullptr, 0);
	pixels::fillRgb565WireFromRgb888Bytes(nullptr, nullptr, 0);
	pixels::fillRepeatedRgb565WirePixel(nullptr, 0, 0);
	pixels::fillRepeatedRgb888Pixel(nullptr, 0, 0);
	// Усечение младших битов произвольного RGB888, не только расширенного RGB565.
	std::uint8_t rgb[]{0xAB, 0xCD, 0xEF, 0x07, 0x03, 0x07};
	std::uint8_t wire[4]{};
	pixels::fillRgb565WireFromRgb888Bytes(wire, rgb, 2);
	check(wire[0] == 0xAE && wire[1] == 0x7D && wire[2] == 0 && wire[3] == 0);
}

} // namespace

int main() {
	streaming();
	exhaustiveColors();
	bursts();
}
