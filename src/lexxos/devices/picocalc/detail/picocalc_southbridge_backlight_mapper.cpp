#include <lexxos/devices/picocalc/detail/picocalc_southbridge_backlight_mapper.h>

namespace lexxos::devices::detail {
namespace {

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	constexpr std::uint8_t jcsLcdBacklightMaxStep = 9;
	constexpr std::uint8_t jcsKeyboardBacklightMaxStep = 3;

	std::uint8_t normalizeJcsBacklight(std::uint8_t value, std::uint8_t maxStep) {
		if (value <= maxStep) {
			return value;
		}
		return static_cast<std::uint8_t>((static_cast<std::uint16_t>(value) * maxStep + 254U) / 255U);
	}
#else
	constexpr std::uint8_t keyboardBacklightStep = 32;
	constexpr std::uint8_t keyboardBacklightMax = 224;
#endif

} // namespace

std::uint8_t PicoCalcSouthbridgeBacklightMapper::normalizeLcd(std::uint8_t value) {
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	return normalizeJcsBacklight(value, jcsLcdBacklightMaxStep);
#else
	return value;
#endif
}

std::uint8_t PicoCalcSouthbridgeBacklightMapper::normalizeKeyboard(std::uint8_t value) {
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	return normalizeJcsBacklight(value, jcsKeyboardBacklightMaxStep);
#else
	if (value == 0) {
		return 0;
	}
	if (value >= keyboardBacklightMax) {
		return keyboardBacklightMax;
	}
	return static_cast<std::uint8_t>(((value + keyboardBacklightStep - 1U) / keyboardBacklightStep) * keyboardBacklightStep);
#endif
}

} // namespace lexxos::devices::detail
