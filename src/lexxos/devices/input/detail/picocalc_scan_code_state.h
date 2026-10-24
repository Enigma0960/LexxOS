#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lexxos::devices::input_detail {

class PicoCalcScanCodeState final {
public:
	static constexpr std::size_t scanCodeCount = 256;
	static constexpr std::size_t storageBytes = scanCodeCount / 8;

	void clear();

	[[nodiscard]] bool anyPressed() const;
	[[nodiscard]] bool isPressed(std::uint8_t code) const;
	void setPressed(std::uint8_t code, bool pressed);

private:
	std::array<std::uint8_t, storageBytes> _pressed{};
};

} // namespace lexxos::devices::input_detail
