#include <lexxos/app/program_input.h>

#include <lexxos/app/detail/program_input_queue.h>
#include <lexxos/lifecycle/event_bus.h>

namespace lexxos::app::program_input {
namespace {

	detail::ProgramInputQueue& queue() {
		static detail::ProgramInputQueue instance;
		return instance;
	}

} // namespace

void publish(lexxos::devices::InputEvent const& event) {
	queue().publish(event);
	(void) lexxos::lifecycle::defaultEventBus().publish({
	    .type = lexxos::lifecycle::EventType::Input,
	    .timestampMs = event.timestampMs,
	    .input = event,
	});
}

void reset(EventCursor& cursor) {
	queue().reset(cursor);
}

std::uint32_t sequence() {
	return queue().sequence();
}

bool pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event) {
	return queue().pollEvent(cursor, event);
}

bool latestEvent(lexxos::devices::InputEvent& event) {
	return queue().latestEvent(event);
}

} // namespace lexxos::app::program_input
