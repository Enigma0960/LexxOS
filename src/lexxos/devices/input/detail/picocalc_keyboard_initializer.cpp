#include <lexxos/devices/input/detail/picocalc_keyboard_initializer.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::input_detail {

PicoCalcKeyboardInitializeResult PicoCalcKeyboardInitializer::initialize(
    PicoCalcKeyboardTransport const& transport,
    PicoCalcSouthbridgeService& service,
    lexxos::platform::PlatformClockService const& clock) const {
	PicoCalcKeyboardInitializeResult result{};
	result.stateReadOk = transport.readStateWithRetry(result.state, retryCount, retryDelayMs, clock);
	if (!result.stateReadOk) {
		return result;
	}

	auto const stateFifoCount = result.state.fifoCount();
	auto const legacyFifoCount = result.state.legacyFifoCount();
	result.pendingFifoCount = stateFifoCount != 0 ? stateFifoCount : legacyFifoCount;

	service.publishKeyboardState(result.state, _uptimeMs(clock));

	std::uint8_t drainEvents = result.pendingFifoCount;
	if (drainEvents > drainEventLimit) {
		drainEvents = drainEventLimit;
	}
	if (drainEvents != 0) {
		result.drainOk = transport.drainFifo(drainEvents);
	}
	return result;
}

std::uint32_t PicoCalcKeyboardInitializer::_uptimeMs(lexxos::platform::PlatformClockService const& clock) {
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

} // namespace lexxos::devices::input_detail
