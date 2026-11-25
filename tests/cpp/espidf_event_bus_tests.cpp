#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <lexxos/lifecycle/event_bus.h>

namespace {

std::atomic_uint taskLocks{}, isrLocks{};
thread_local portMUX_TYPE* held{};

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

void testEnterCritical(portMUX_TYPE* mux, bool isr) {
	expect(!held, "recursive critical section");
	mux->lock();
	held = mux;
	if (isr) {
		++isrLocks;
	} else {
		++taskLocks;
	}
}

void testExitCritical(portMUX_TYPE* mux, bool) {
	expect(held == mux, "different critical section on exit");
	held = nullptr;
	mux->unlock();
}

int main() {
	namespace lifecycle = lexxos::lifecycle;
	lifecycle::EventBus bus;
	lifecycle::EventCursor cursor;
	bus.reset(cursor);

	auto produce = [&bus](bool isr) {
		for (unsigned i = 0; i < 2000; ++i) {
			lifecycle::Event event{.type = lifecycle::EventType::Input, .timestampMs = i, .asserted = (i % 2 == 0)};
			expect(isr ? bus.publishFromIsr(event) : bus.publish(event), "publish failed");
		}
	};

	std::thread task(produce, false);
	std::thread interrupt(produce, true);
	// Чтение одновременно с публикацией проверяет целостность составного payload.
	std::thread reader([&bus] {
		for (unsigned i = 0; i < 2000; ++i) {
			lifecycle::Event event;
			if (bus.latest(event)) {
				expect(event.asserted == (event.timestampMs % 2 == 0), "torn payload");
			}
		}
	});

	task.join();
	interrupt.join();
	reader.join();
	expect(isrLocks == 2000 && taskLocks >= 2001, "IDF critical sections not used");
	expect(bus.sequence() == 4000, "lost concurrent publication");
	unsigned count = 0;
	lifecycle::Event event;
	while (bus.poll(cursor, event)) {
		expect(event.type == lifecycle::EventType::Input && event.asserted == (event.timestampMs % 2 == 0), "corrupt ring payload");
		++count;
	}

	expect(count == lifecycle::EventBus::queueCapacity, "overflow policy changed");
	bus.reset(cursor);
	expect(!bus.poll(cursor, event), "reset cursor replayed history");
	lifecycle::EventBus independent;
	expect(independent.sequence() == 0 && !independent.latest(event), "event buses share storage");
}
