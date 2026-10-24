#pragma once

#include <cstdint>

namespace lexxos::shell::detail {

enum class ShellInputKey : std::uint8_t {
	ArrowUp,
	ArrowDown,
	ArrowRight,
	ArrowLeft,
	Home,
	End,
	Delete,
};

struct ShellEscapeSequenceResult {
	bool hasKey{false};
	ShellInputKey key{ShellInputKey::ArrowUp};
};

class ShellEscapeSequenceParser final {
public:
	void start();
	void reset();

	[[nodiscard]] bool active() const;
	[[nodiscard]] ShellEscapeSequenceResult consume(char value);

private:
	enum class State : std::uint8_t {
		None,
		Escape,
		Csi,
		Ss3,
	};

	void _enterCsi();
	[[nodiscard]] ShellEscapeSequenceResult _consumeEscape(char value);
	[[nodiscard]] ShellEscapeSequenceResult _consumeSs3(char value);
	[[nodiscard]] ShellEscapeSequenceResult _consumeCsi(char value);

	State _state{State::None};
	std::uint16_t _parameter{0};
	bool _parameterPresent{false};
	bool _ignoreParameters{false};
};

} // namespace lexxos::shell::detail
