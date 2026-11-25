#pragma once
#include <span>

#include <lexxos/drivers/core/display/ili9488_init.h>

namespace lexxos::drivers::core {

enum class Ili9488PixelSource {
	Rgb565Words,
	Rgb565Bytes,
	Rgb888Bytes
};

struct Ili9488PixelTransport {
	void* context{nullptr};
	// Синхронная передача: callback завершает чтение буфера до возврата.
	bool (*write)(void*, std::uint8_t const*, std::size_t){nullptr};
};

// Caller владеет CS/шиной и неперекрывающимися входом и scratch. Words выровнены как uint16_t.
// Совпадающие byte-форматы передаются напрямую одним вызовом, без scratch.
bool streamIli9488Pixels(Ili9488PixelTransport const& transport, Ili9488PixelFormat wire,
    Ili9488PixelSource source, void const* data, std::size_t count, std::span<std::uint8_t> scratch, std::size_t burstPixels);
bool fillIli9488Pixels(Ili9488PixelTransport const& transport, Ili9488PixelFormat wire,
    std::uint16_t color, std::size_t count, std::span<std::uint8_t> scratch, std::size_t burstPixels);

} // namespace lexxos::drivers::core
