#include <lexxos/drivers/core/display/ili9488_pixels.h>

namespace lexxos::drivers::core {

namespace {

	std::uint8_t highByte(std::uint16_t value) {
		return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	}

	std::uint8_t lowByte(std::uint16_t value) {
		return static_cast<std::uint8_t>(value & 0xFFU);
	}

	std::uint16_t rgb888ToRgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
		return static_cast<std::uint16_t>(
		    ((static_cast<std::uint16_t>(red) & 0xF8U) << 8U)
		    | ((static_cast<std::uint16_t>(green) & 0xFCU) << 3U)
		    | (static_cast<std::uint16_t>(blue) >> 3U));
	}

	std::uint8_t rgb565Red8(std::uint16_t value) {
		auto const red = static_cast<std::uint8_t>((value >> 11U) & 0x1FU);
		return static_cast<std::uint8_t>((red << 3U) | (red >> 2U));
	}

	std::uint8_t rgb565Green8(std::uint16_t value) {
		auto const green = static_cast<std::uint8_t>((value >> 5U) & 0x3FU);
		return static_cast<std::uint8_t>((green << 2U) | (green >> 4U));
	}

	std::uint8_t rgb565Blue8(std::uint16_t value) {
		auto const blue = static_cast<std::uint8_t>(value & 0x1FU);
		return static_cast<std::uint8_t>((blue << 3U) | (blue >> 2U));
	}

	void appendRgb565AsRgb888(std::uint8_t*& output, std::uint16_t value) {
		*output++ = rgb565Red8(value);
		*output++ = rgb565Green8(value);
		*output++ = rgb565Blue8(value);
	}

	void appendRgb565AsRgb565(std::uint8_t*& output, std::uint16_t value) {
		*output++ = highByte(value);
		*output++ = lowByte(value);
	}

} // namespace

void fillRgb888FromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, std::size_t count) {
	auto* out = output;
	for (std::size_t index = 0; index < count; ++index) {
		appendRgb565AsRgb888(out, pixels[index]);
	}
}

void fillRgb888FromRgb565Bytes(std::uint8_t* output, std::uint8_t const* bytes, std::size_t pixelCount) {
	auto* out = output;
	for (std::size_t index = 0; index < pixelCount; ++index) {
		auto const base = static_cast<std::size_t>(index * 2);
		auto const pixel = static_cast<std::uint16_t>(
		    (static_cast<std::uint16_t>(bytes[base]) << 8U) | bytes[base + 1U]);
		appendRgb565AsRgb888(out, pixel);
	}
}

void fillRgb565WireFromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, std::size_t count) {
	auto* out = output;
	for (std::size_t index = 0; index < count; ++index) {
		appendRgb565AsRgb565(out, pixels[index]);
	}
}

void fillRgb565WireFromRgb888Bytes(std::uint8_t* output, std::uint8_t const* bytes, std::size_t pixelCount) {
	auto* out = output;
	for (std::size_t index = 0; index < pixelCount; ++index) {
		auto const base = static_cast<std::size_t>(index * 3);
		appendRgb565AsRgb565(out, rgb888ToRgb565(bytes[base], bytes[base + 1U], bytes[base + 2U]));
	}
}

void fillRepeatedRgb565WirePixel(std::uint8_t* output, std::uint16_t rgb565, std::size_t count) {
	auto* out = output;
	for (std::size_t index = 0; index < count; ++index) {
		appendRgb565AsRgb565(out, rgb565);
	}
}

bool rgb565ExpandsToRepeatedRgb888Byte(std::uint16_t rgb565, std::uint8_t& value) {
	auto const red = rgb565Red8(rgb565);
	auto const green = rgb565Green8(rgb565);
	auto const blue = rgb565Blue8(rgb565);
	if (red != green || red != blue) {
		return false;
	}

	value = red;
	return true;
}

void fillRepeatedRgb888Pixel(std::uint8_t* output, std::uint16_t rgb565, std::size_t count) {
	auto* out = output;
	for (std::size_t index = 0; index < count; ++index) {
		appendRgb565AsRgb888(out, rgb565);
	}
}

} // namespace lexxos::drivers::core
