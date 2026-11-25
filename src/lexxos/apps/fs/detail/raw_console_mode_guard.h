#pragma once

#include <lexxos/app/console_input_service.h>

namespace lexxos::apps::filesystem_programs::detail::ymodem {

class RawConsoleModeGuard final {
public:
	explicit RawConsoleModeGuard(lexxos::app::ConsoleInputService& input);

	RawConsoleModeGuard(RawConsoleModeGuard const&) = delete;
	RawConsoleModeGuard& operator=(RawConsoleModeGuard const&) = delete;

	~RawConsoleModeGuard();

private:
	lexxos::app::ConsoleInputService& _input;
	lexxos::app::console_input::Mode _previous;
};

} // namespace lexxos::apps::filesystem_programs::detail::ymodem
