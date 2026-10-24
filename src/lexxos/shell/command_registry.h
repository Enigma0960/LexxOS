#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include <lexxos/shell/command.h>

namespace lexxos::shell {

class CommandRegistry {
public:
	static constexpr std::size_t maxCommands = 48;

	bool add(Command command);
	Command const* find(std::string_view name) const;
	std::size_t size() const;
	Command const& at(std::size_t index) const;

private:
	std::array<Command, maxCommands> _commands{};
	std::size_t _size{0};
};

} // namespace lexxos::shell
