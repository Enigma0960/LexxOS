#pragma once

#include <cstdint>

#include <lexxos/drivers/core/display/ili9488_init.h>

namespace lexxos::devices {

// Уровень reset задаётся логически; распиновкой управляет backend.
struct Ili9488ResetPolicy {
	bool enabled{true};
	bool activeLow{true};
	std::uint32_t beforeAssertMs{10};
	std::uint32_t assertedMs{10};
	std::uint32_t recoveryMs{200};
};

// Параметры панели без распиновки и номеров физических контроллеров.
struct Ili9488Panel {
	int width{};
	int height{};
	int columnOffset{};
	int rowOffset{};
	bool invertColors{};
	drivers::core::Ili9488PixelFormat wireFormat{drivers::core::Ili9488PixelFormat::Rgb666};
	Ili9488ResetPolicy reset{};
};

} // namespace lexxos::devices
