#pragma once

#include <string_view>

#include <lexxos/hardware/products/dev_pico_display.h>
#include <lexxos/hardware/products/dev_pico_input.h>
#include <lexxos/hardware/products/dev_pico_storage.h>

namespace lexxos::hardware::products::dev_pico {

// Identity продукта не зависит от выбранной стадии загрузки.
inline constexpr char id[] = "dev_pico";
inline constexpr char name[] = "LexxOS development board on Raspberry Pi Pico";
inline constexpr char vendor[] = "LexxOS";
inline constexpr char processor[] = "RP2040";

// Декларации компонентов не могут ссылаться на неизвестную шину/экспандер.
static_assert(sdCard.endpoint.controller.index == storageSpi.controller.index);
static_assert(psram.endpoint.controller.index == peripheralSpi.controller.index);
static_assert(display.endpoint.controller.index == peripheralSpi.controller.index);
static_assert(keyboardExpander.bus.index == debugI2c.controller.index);
static_assert(std::string_view{keyboard.expanderName} == keyboardExpander.name);
static_assert(std::string_view{segaGamepad.expanderName} == keyboardExpander.name);

} // namespace lexxos::hardware::products::dev_pico
