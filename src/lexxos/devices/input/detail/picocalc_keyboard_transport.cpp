#include <lexxos/devices/input/detail/picocalc_keyboard_transport.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::input_detail {

PicoCalcKeyboardTransport::PicoCalcKeyboardTransport(PicoCalcSouthbridge& southbridge)
    : _southbridge(southbridge) {
}

bool PicoCalcKeyboardTransport::readState(PicoCalcKeyboardState& state) const {
	return _southbridge.readKeyboardState(state);
}

bool PicoCalcKeyboardTransport::readStateWithRetry(
    PicoCalcKeyboardState& state,
    std::uint8_t retryCount,
    std::uint32_t retryDelayMs,
    lexxos::platform::PlatformClockService const& clock) const {
	for (std::uint8_t attempt = 0; attempt < retryCount; ++attempt) {
		if (readState(state)) {
			return true;
		}
		_sleepMs(clock, retryDelayMs);
	}
	return false;
}

bool PicoCalcKeyboardTransport::readFifo(PicoCalcKeyboardFifoEvent& event) const {
	return _southbridge.readKeyboardFifo(event);
}

bool PicoCalcKeyboardTransport::drainFifo(std::uint8_t maxEvents) const {
	for (std::uint8_t index = 0; index < maxEvents; ++index) {
		PicoCalcKeyboardFifoEvent event{};
		if (!readFifo(event)) {
			return false;
		}
		if (event.isEmpty()) {
			return true;
		}
	}
	return true;
}

void PicoCalcKeyboardTransport::_sleepMs(
    lexxos::platform::PlatformClockService const& clock,
    std::uint32_t delayMs) {
	if (delayMs == 0 || clock.sleepMs == nullptr) {
		return;
	}
	clock.sleepMs(delayMs);
}

} // namespace lexxos::devices::input_detail
