#pragma once

#include <array>

#include <lexxos/app/detail/program_input_queue.h>
#include <lexxos/lifecycle/event_bus.h>

namespace lexxos::app {

class ProgramInputService final {
public:
	using InputMode = program_input::InputMode;
	using EventCursor = program_input::EventCursor;
	explicit ProgramInputService(lifecycle::EventBus& events);
	ProgramInputService(ProgramInputService const&) = delete;
	ProgramInputService& operator=(ProgramInputService const&) = delete;
	[[nodiscard]] lifecycle::EventBus& events();
	[[nodiscard]] bool setInputMode(std::uint32_t processId, InputMode mode);
	[[nodiscard]] InputMode inputMode(std::uint32_t processId);
	void releaseInputMode(std::uint32_t processId);
	void publish(devices::InputEvent const& event);
	void reset(EventCursor& cursor);
	[[nodiscard]] std::uint32_t sequence();
	[[nodiscard]] bool pollEvent(EventCursor& cursor, devices::InputEvent& event);
	[[nodiscard]] bool latestEvent(devices::InputEvent& event);

private:
	void lockModes();
	void unlockModes();

	struct ProcessMode {
		std::uint32_t id{0};
		InputMode mode{InputMode::Terminal};
	};

	lifecycle::EventBus& _events;
	program_input::detail::ProgramInputQueue _queue;
	std::array<ProcessMode, runtime::standardRuntimeProfile.programs.maxProcesses> _processModes{};
#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	std::mutex _processModesMutex;
#endif
};

} // namespace lexxos::app
