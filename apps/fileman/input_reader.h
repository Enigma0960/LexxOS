#pragma once

// UART-oriented input for console .lexx applications.
// LexxOS shell forwards both normalized InputEvents and terminal bytes, so do
// not consume both sources: that would process every key twice.
namespace fileman {

enum class InputKey {
	None,
	Up,
	Down,
	Left,
	Right,
	Enter,
	Backspace,
	Tab,
	View,
	Refresh,
	Options,
	Copy,
	Move,
	Rename,
	Delete,
	PageUp,
	PageDown,
	Escape,
	Quit,
};

class InputReader {
public:
	InputKey poll();

private:
	enum class State {
		Plain,
		Escape,
		Csi,
		Ss3
	};

	InputKey consume(unsigned char character);
	State _state{State::Plain};
	unsigned _idlePolls{0};
	unsigned _parameter{0};
};

} // namespace fileman
