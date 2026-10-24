#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::shell {
class Shell;
} // namespace lexxos::shell

namespace lexxos::shell::detail {

struct PathCompletionContext {
	std::string_view prefix{};
	std::array<char, lexxos::vfs::maxNameLength + 1> commonName{};
	std::size_t commonLength{0};
	std::size_t matchCount{0};
	lexxos::vfs::NodeType singleType{lexxos::vfs::NodeType::Missing};
};

struct PathCompletionMatchWriterContext {
	Shell* shell{nullptr};
	std::string_view prefix{};
};

} // namespace lexxos::shell::detail
