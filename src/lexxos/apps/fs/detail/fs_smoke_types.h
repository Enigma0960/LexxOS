#pragma once

#include <array>
#include <cstddef>

namespace lexxos::apps::filesystem_programs::detail {

struct SmokeReadBuffer {
	std::array<char, 64> data{};
	std::size_t size{0};
	bool overflow{false};
};

} // namespace lexxos::apps::filesystem_programs::detail
