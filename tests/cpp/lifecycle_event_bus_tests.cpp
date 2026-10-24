#include <lexxos/app/program_input.h>
#include <lexxos/lifecycle/event_bus.h>

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "lifecycle event bus test failed: " << message << '\n';
		std::exit(1);
	}
}

lexxos::devices::InputEvent keyEvent(lexxos::devices::KeyCode key, std::uint32_t timestampMs) {
	return {
	    .type = lexxos::devices::InputEventType::KeyDown,
	    .key = key,
	    .timestampMs = timestampMs,
	};
}

void checkPublishAndPollOrder() {
	lexxos::lifecycle::EventBus bus;
	lexxos::lifecycle::EventCursor cursor;
	bus.reset(cursor);

	expect(bus.publish({.type = lexxos::lifecycle::EventType::SdCardInserted, .timestampMs = 10}), "publish inserted");
	expect(bus.publish({.type = lexxos::lifecycle::EventType::SdCardRemoved, .timestampMs = 20}), "publish removed");

	lexxos::lifecycle::Event event{};
	expect(bus.poll(cursor, event), "first event should be readable");
	expect(event.type == lexxos::lifecycle::EventType::SdCardInserted, "first event type");
	expect(event.timestampMs == 10, "first event timestamp");

	expect(bus.poll(cursor, event), "second event should be readable");
	expect(event.type == lexxos::lifecycle::EventType::SdCardRemoved, "second event type");
	expect(event.timestampMs == 20, "second event timestamp");

	expect(!bus.poll(cursor, event), "cursor should reach queue tail");
}

void checkProgramInputPublishesLifecycleEvent() {
	auto& bus = lexxos::lifecycle::defaultEventBus();
	lexxos::lifecycle::EventCursor cursor;
	bus.reset(cursor);

	lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::A, 123));

	lexxos::lifecycle::Event event{};
	expect(bus.poll(cursor, event), "program input should publish lifecycle event");
	expect(event.type == lexxos::lifecycle::EventType::Input, "program input lifecycle event type");
	expect(event.input.key == lexxos::devices::KeyCode::A, "program input lifecycle event key");
	expect(event.timestampMs == 123, "program input lifecycle event timestamp");
}

} // namespace

void checkLifecycleEventBus() {
	checkPublishAndPollOrder();
	checkProgramInputPublishesLifecycleEvent();
}
