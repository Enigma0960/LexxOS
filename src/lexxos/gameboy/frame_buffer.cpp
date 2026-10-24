#include <lexxos/gameboy/frame_buffer.h>

namespace lexxos::gameboy {

void FrameBuffer::clear(std::uint8_t shade) {
	auto const packedShade = static_cast<std::uint8_t>(shade & 0x03U);
	auto const packedByte = static_cast<std::uint8_t>(
	    packedShade | static_cast<std::uint8_t>(packedShade << 2U) | static_cast<std::uint8_t>(packedShade << 4U) | static_cast<std::uint8_t>(packedShade << 6U));
	_pixels.fill(packedByte);
}

void FrameBuffer::setPixel(int x, int y, std::uint8_t shade) {
	if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
		return;
	}

	auto const pixelIndex = static_cast<std::size_t>(y * screenWidth + x);
	auto const byteIndex = pixelIndex / pixelsPerPackedByte;
	auto const shift = static_cast<unsigned>((pixelIndex % pixelsPerPackedByte) * 2U);
	auto const mask = static_cast<std::uint8_t>(0x03U << shift);
	auto const value = static_cast<std::uint8_t>((shade & 0x03U) << shift);
	_pixels[byteIndex] = static_cast<std::uint8_t>((_pixels[byteIndex] & static_cast<std::uint8_t>(~mask)) | value);
}

std::uint8_t FrameBuffer::pixel(int x, int y) const {
	if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
		return 0;
	}

	auto const pixelIndex = static_cast<std::size_t>(y * screenWidth + x);
	auto const byteIndex = pixelIndex / pixelsPerPackedByte;
	auto const shift = static_cast<unsigned>((pixelIndex % pixelsPerPackedByte) * 2U);
	return static_cast<std::uint8_t>((_pixels[byteIndex] >> shift) & 0x03U);
}

std::uint8_t const* FrameBuffer::data() const {
	return _pixels.data();
}

} // namespace lexxos::gameboy
