#pragma once

#include <lexxos/hardware/modules/rp2040.h>

namespace lexxos::hardware::products::dev_pico {

inline constexpr modules::rp2040::Pin statusLed{25};
inline constexpr modules::rp2040::UartRoute debugUart{{0}, {0}, {1}};
inline constexpr modules::rp2040::I2cRoute debugI2c{{0}, {20}, {21}};
inline constexpr modules::rp2040::SpiRoute storageSpi{{0}, {18}, {19}, {16}};
inline constexpr modules::rp2040::SpiRoute peripheralSpi{{1}, {10}, {11}, {12}};

static_assert(modules::rp2040::valid(debugUart));
static_assert(modules::rp2040::valid(debugI2c));
static_assert(modules::rp2040::valid(storageSpi));
static_assert(modules::rp2040::valid(peripheralSpi));

} // namespace lexxos::hardware::products::dev_pico
