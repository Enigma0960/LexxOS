#pragma once
#include <cstddef>
#include <cstdint>

namespace lexxos::drivers::core {

// Чистые преобразования без владения памятью. Буферы не перекрываются.
// Caller предоставляет count пикселей входа и 2/3 байта выхода на пиксель.
// RGB565 bytes и wire — старший байт первым; RGB888 — последовательность R,G,B.
// При count == 0 буферы не разыменовываются.
void fillRgb888FromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, std::size_t count);
void fillRgb888FromRgb565Bytes(std::uint8_t* output, std::uint8_t const* bytes, std::size_t pixelCount);
void fillRgb565WireFromRgb565Pixels(std::uint8_t* output, std::uint16_t const* pixels, std::size_t count);
void fillRgb565WireFromRgb888Bytes(std::uint8_t* output, std::uint8_t const* bytes, std::size_t pixelCount);
void fillRepeatedRgb565WirePixel(std::uint8_t* output, std::uint16_t rgb565, std::size_t count);
bool rgb565ExpandsToRepeatedRgb888Byte(std::uint16_t rgb565, std::uint8_t& value);
void fillRepeatedRgb888Pixel(std::uint8_t* output, std::uint16_t rgb565, std::size_t count);

} // namespace lexxos::drivers::core
