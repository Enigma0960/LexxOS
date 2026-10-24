#pragma once

#include <cstdint>

namespace lexxos::gameboy {

inline constexpr std::uint8_t interruptVBlank = 0x01;
inline constexpr std::uint8_t interruptLcdStat = 0x02;
inline constexpr std::uint8_t interruptTimer = 0x04;
inline constexpr std::uint8_t interruptSerial = 0x08;
inline constexpr std::uint8_t interruptJoypad = 0x10;

struct JoypadState {
	bool right{false};
	bool left{false};
	bool up{false};
	bool down{false};
	bool a{false};
	bool b{false};
	bool select{false};
	bool start{false};
};

} // namespace lexxos::gameboy
