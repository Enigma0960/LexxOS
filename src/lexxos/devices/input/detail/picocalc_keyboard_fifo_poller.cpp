#include <lexxos/devices/input/detail/picocalc_keyboard_fifo_poller.h>

namespace lexxos::devices::input_detail {

PicoCalcKeyboardFifoPollResult PicoCalcKeyboardFifoPoller::poll(
    PicoCalcKeyboardTransport const& transport,
    EventBuffer& events) const {
	PicoCalcKeyboardFifoPollResult result{};
	for (std::uint8_t index = 0; index < readLimit; ++index) {
		PicoCalcKeyboardFifoEvent event{};
		if (!transport.readFifo(event)) {
			result.communicationOk = false;
			return result;
		}

		++result.successfulReads;
		if (event.isEmpty()) {
			result.lastStatus = 0;
			return result;
		}

		result.lastStatus = event.status;
		events[result.eventCount] = event;
		++result.eventCount;
	}
	return result;
}

} // namespace lexxos::devices::input_detail
