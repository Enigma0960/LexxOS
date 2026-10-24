#pragma once

#include <array>
#include <cstdint>

#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	#include <mutex>
#endif

#include <lexxos/devices/input_device.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::lifecycle {

enum class EventType : std::uint8_t {
	None,
	SdCardDetectChanged,
	SdCardInserted,
	SdCardRemoved,
	Input,
	PowerOffRequested,
	ShutdownRequested,
};

struct Event {
	EventType type{EventType::None};
	std::uint32_t timestampMs{0};
	bool asserted{false};
	lexxos::devices::InputEvent input{};
};

struct EventCursor {
	std::uint32_t nextSequence{0};
};

class EventBus final {
public:
	static constexpr std::uint32_t queueCapacity =
	    lexxos::runtime::standardRuntimeProfile.events.lifecycleQueueCapacity;

	void reset(EventCursor& cursor);
	[[nodiscard]] std::uint32_t sequence();
	[[nodiscard]] bool publish(Event const& event);
	[[nodiscard]] bool publishFromIsr(Event const& event);
	[[nodiscard]] bool poll(EventCursor& cursor, Event& event);
	[[nodiscard]] bool latest(Event& event);

private:
	void lock();
	void unlock();
	[[nodiscard]] bool pushUnlocked(Event const& event);
	[[nodiscard]] std::uint32_t newestSequence() const;
	[[nodiscard]] std::uint32_t oldestAvailableSequence() const;
	void resetAfterSequenceWrap();

	std::array<std::uint32_t, queueCapacity> _sequences{};
	std::array<Event, queueCapacity> _events{};
	std::uint32_t _nextSequence{1};
#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	std::mutex _lock;
#endif
};

[[nodiscard]] EventBus& defaultEventBus();

} // namespace lexxos::lifecycle
