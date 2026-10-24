#include <lexxos/app/program_input.h>

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "program input test failed: " << message << '\n';
		std::exit(1);
	}
}

lexxos::devices::InputEvent keyEvent(
    lexxos::devices::KeyCode key,
    std::uint32_t timestampMs,
    lexxos::devices::InputEventType type = lexxos::devices::InputEventType::KeyDown) {
	return {
	    .type = type,
	    .key = key,
	    .timestampMs = timestampMs,
	};
}

void checkPublishAndPollOrder() {
	lexxos::app::program_input::EventCursor cursor;
	lexxos::app::program_input::reset(cursor);

	lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::A, 10));
	lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::B, 20));

	lexxos::devices::InputEvent event{};
	expect(lexxos::app::program_input::pollEvent(cursor, event), "first event should be readable");
	expect(event.key == lexxos::devices::KeyCode::A, "first event key");
	expect(event.timestampMs == 10, "first event timestamp");

	expect(lexxos::app::program_input::pollEvent(cursor, event), "second event should be readable");
	expect(event.key == lexxos::devices::KeyCode::B, "second event key");
	expect(event.timestampMs == 20, "second event timestamp");

	expect(!lexxos::app::program_input::pollEvent(cursor, event), "cursor should reach queue tail");
}

void checkLatestEvent() {
	lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::C, 30));
	lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::D, 40, lexxos::devices::InputEventType::KeyUp));

	lexxos::devices::InputEvent event{};
	expect(lexxos::app::program_input::latestEvent(event), "latest event should be readable");
	expect(event.type == lexxos::devices::InputEventType::KeyUp, "latest event type");
	expect(event.key == lexxos::devices::KeyCode::D, "latest event key");
	expect(event.timestampMs == 40, "latest event timestamp");
}

void checkStaleCursorSkipsToOldestAvailableEvent() {
	lexxos::app::program_input::EventCursor cursor;
	cursor.nextSequence = 1;

	for (std::uint32_t index = 0; index < 40; ++index) {
		lexxos::app::program_input::publish(keyEvent(lexxos::devices::KeyCode::E, 100 + index));
	}

	lexxos::devices::InputEvent event{};
	expect(lexxos::app::program_input::pollEvent(cursor, event), "stale cursor should recover to oldest event");
	expect(event.key == lexxos::devices::KeyCode::E, "recovered event key");
	expect(event.timestampMs > 100, "recovered event should skip overwritten entries");
}

} // namespace

void checkProgramInput() {
	checkPublishAndPollOrder();
	checkLatestEvent();
	checkStaleCursorSkipsToOldestAvailableEvent();
}
