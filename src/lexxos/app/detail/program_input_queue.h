#pragma once

#include <array>
#include <cstdint>

#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	#include <mutex>
#endif

#include <lexxos/app/program_input.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::app::program_input::detail {

class ProgramInputQueue final {
public:
	void publish(lexxos::devices::InputEvent const& event);
	void reset(EventCursor& cursor);
	[[nodiscard]] std::uint32_t sequence();
	[[nodiscard]] bool pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event);
	[[nodiscard]] bool latestEvent(lexxos::devices::InputEvent& event);

private:
	void lock();
	void unlock();
	[[nodiscard]] std::uint32_t newestSequence() const;
	[[nodiscard]] std::uint32_t oldestAvailableSequence() const;
	void resetAfterSequenceWrap();

	std::array<std::uint32_t, lexxos::app::program_input::queueCapacity> _sequences{};
	std::array<lexxos::devices::InputEvent, lexxos::app::program_input::queueCapacity> _events{};
	std::uint32_t _nextSequence{1};
#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	std::mutex _lock;
#endif
};

} // namespace lexxos::app::program_input::detail
