#pragma once

#include <cstdint>

namespace lexxos::hardware::products::dev_pico {

// Политики продукта не определяют физические GPIO и маршруты шин.
inline constexpr std::uint32_t debugUartBaudRate = 115200;
inline constexpr std::uint32_t debugI2cBaudRate = 400000;
inline constexpr std::uint32_t storageSpiBaudRate = 50000000;
inline constexpr std::uint32_t psramSpiBaudRate = 1000000;
inline constexpr std::uint32_t keyboardScanMs = 5;
inline constexpr std::uint32_t keyboardDebounceMs = 20;

} // namespace lexxos::hardware::products::dev_pico
