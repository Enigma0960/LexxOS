#include <lexxos/drivers/core/display/ili9488_stream.h>

#include <limits>

#include <lexxos/drivers/core/display/ili9488_pixels.h>

namespace lexxos::drivers::core {

namespace {

	std::size_t bytesPerPixel(Ili9488PixelFormat wire) {
		switch (wire) {
			case Ili9488PixelFormat::Rgb565: return 2;
			case Ili9488PixelFormat::Rgb666: return 3;
		}
		return 0;
	}

	bool validBuffer(std::span<std::uint8_t> scratch, std::size_t burst, std::size_t bytes) {
		return scratch.data() && burst && burst <= scratch.size() / bytes;
	}

} // namespace

bool streamIli9488Pixels(Ili9488PixelTransport const& transport, Ili9488PixelFormat wire,
    Ili9488PixelSource source, void const* data, std::size_t count, std::span<std::uint8_t> scratch, std::size_t burstPixels) {
	auto bytes = bytesPerPixel(wire);
	if (!transport.write || !bytes || count > std::numeric_limits<std::size_t>::max() / 3 || (source != Ili9488PixelSource::Rgb565Words && source != Ili9488PixelSource::Rgb565Bytes && source != Ili9488PixelSource::Rgb888Bytes)) {
		return false;
	}

	if (!count) {
		return true;
	}

	if (!data) {
		return false;
	}

	auto const* input = static_cast<std::uint8_t const*>(data);
	bool direct = (source == Ili9488PixelSource::Rgb565Bytes && bytes == 2) || (source == Ili9488PixelSource::Rgb888Bytes && bytes == 3);
	if (direct) {
		return transport.write(transport.context, input, count * bytes);
	}

	if (!validBuffer(scratch, burstPixels, bytes)) {
		return false;
	}

	for (std::size_t offset = 0; offset < count;) {
		auto chunk = count - offset < burstPixels ? count - offset : burstPixels;
		switch (source) {
			case Ili9488PixelSource::Rgb565Words:
				if (bytes == 2) {
					fillRgb565WireFromRgb565Pixels(scratch.data(), static_cast<std::uint16_t const*>(data) + offset, chunk);
				} else {
					fillRgb888FromRgb565Pixels(scratch.data(), static_cast<std::uint16_t const*>(data) + offset, chunk);
				}

				break;
			case Ili9488PixelSource::Rgb565Bytes: fillRgb888FromRgb565Bytes(scratch.data(), input + offset * 2, chunk); break;
			case Ili9488PixelSource::Rgb888Bytes: fillRgb565WireFromRgb888Bytes(scratch.data(), input + offset * 3, chunk); break;
		}

		if (!transport.write(transport.context, scratch.data(), chunk * bytes)) {
			return false;
		}

		offset += chunk;
	}

	return true;
}

bool fillIli9488Pixels(Ili9488PixelTransport const& transport, Ili9488PixelFormat wire,
    std::uint16_t color, std::size_t count, std::span<std::uint8_t> scratch, std::size_t burstPixels) {
	auto bytes = bytesPerPixel(wire);
	if (!transport.write || !bytes || count > std::numeric_limits<std::size_t>::max() / bytes) {
		return false;
	}

	if (!count) {
		return true;
	}

	if (!validBuffer(scratch, burstPixels, bytes)) {
		return false;
	}

	auto prepared = count < burstPixels ? count : burstPixels;
	if (bytes == 2) {
		fillRepeatedRgb565WirePixel(scratch.data(), color, prepared);
	} else {
		std::uint8_t byte{};
		if (rgb565ExpandsToRepeatedRgb888Byte(color, byte)) {
			for (std::size_t i = 0; i < prepared * bytes; ++i) {
				scratch[i] = byte;
			}
		} else {
			fillRepeatedRgb888Pixel(scratch.data(), color, prepared);
		}
	}

	while (count) {
		auto chunk = count < prepared ? count : prepared;
		if (!transport.write(transport.context, scratch.data(), chunk * bytes)) {
			return false;
		}

		count -= chunk;
	}

	return true;
}

} // namespace lexxos::drivers::core
