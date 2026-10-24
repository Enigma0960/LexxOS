#include <lexxos/shell/command_registry.h>

namespace lexxos::shell {

bool CommandRegistry::add(Command command) {
	if (command.name.empty() || command.handler == nullptr || _size >= _commands.size()) {
		return false;
	}

	if (find(command.name) != nullptr) {
		return false;
	}

	_commands[_size] = command;
	++_size;
	return true;
}

Command const* CommandRegistry::find(std::string_view name) const {
	for (std::size_t index = 0; index < _size; ++index) {
		if (_commands[index].name == name) {
			return &_commands[index];
		}
	}

	return nullptr;
}

std::size_t CommandRegistry::size() const {
	return _size;
}

Command const& CommandRegistry::at(std::size_t index) const {
	return _commands[index];
}

} // namespace lexxos::shell
