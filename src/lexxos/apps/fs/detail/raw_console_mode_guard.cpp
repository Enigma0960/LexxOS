#include <lexxos/apps/fs/detail/raw_console_mode_guard.h>

namespace lexxos::apps::filesystem_programs::detail::ymodem {

RawConsoleModeGuard::RawConsoleModeGuard(lexxos::app::ConsoleInputService& input)
    : _input(input)
    , _previous(input.mode()) {
	_input.setMode(lexxos::app::console_input::Mode::RawProgram);
}

RawConsoleModeGuard::~RawConsoleModeGuard() {
	_input.setMode(_previous);
}

} // namespace lexxos::apps::filesystem_programs::detail::ymodem
