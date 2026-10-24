#pragma once

#include <cstddef>
#include <string_view>

namespace lexxos::shell {

class Shell;

struct CommandInvocation {
	int argc{0};
	char const* const* argv{nullptr};
};

struct ShellContext {
	Shell* shell{nullptr};
};

using CommandHandler = int (*)(ShellContext& context, CommandInvocation const& invocation);

struct Command {
	std::string_view name;
	std::string_view help;
	CommandHandler handler{nullptr};
};

} // namespace lexxos::shell
