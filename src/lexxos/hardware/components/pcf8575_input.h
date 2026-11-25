#pragma once

#include <array>
#include <cstdint>

#include <lexxos/hardware/modules/rp2040.h>

namespace lexxos::hardware::components {

struct Pcf8575 {
	char const* name;
	modules::rp2040::I2cId bus;
	std::uint8_t address;
	modules::rp2040::Pin interrupt;
};

struct Pcf8575Matrix {
	char const* name;
	char const* expanderName;
	std::array<std::uint8_t, 6> rows;
	std::array<std::uint8_t, 4> columns;
};

struct SegaGamepad {
	char const* name;
	char const* expanderName;
	std::uint8_t up;
	std::uint8_t down;
	std::uint8_t left;
	std::uint8_t right;
	std::uint8_t buttonBa;
	std::uint8_t buttonCStart;
	modules::rp2040::Pin select;
};

} // namespace lexxos::hardware::components
