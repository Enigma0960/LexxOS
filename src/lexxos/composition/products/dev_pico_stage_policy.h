#pragma once

#include <cstdint>

namespace lexxos::composition::products {

// Переопределение скорости SPI экрана — политика target/stage, не wiring.
#ifndef LEXXOS_ST7735_SPI_BAUDRATE
	#define LEXXOS_ST7735_SPI_BAUDRATE 25000000
#endif

inline constexpr std::uint32_t devPicoDisplaySpiHz = LEXXOS_ST7735_SPI_BAUDRATE;

} // namespace lexxos::composition::products
