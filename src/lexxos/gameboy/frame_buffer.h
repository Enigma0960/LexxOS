#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/gameboy/ppu_types.h>

namespace lexxos::gameboy {

class FrameBuffer final {
public:
	void clear(std::uint8_t shade);
	void setPixel(int x, int y, std::uint8_t shade);
	[[nodiscard]] std::uint8_t pixel(int x, int y) const;
	[[nodiscard]] std::uint8_t const* data() const;

	[[nodiscard]] static constexpr std::size_t storageSize() { return frameBufferPackedSize; }

private:
	std::array<std::uint8_t, frameBufferPackedSize> _pixels{};
};

} // namespace lexxos::gameboy
