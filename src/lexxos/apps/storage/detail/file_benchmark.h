#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

class FileBenchmark final {
public:
	// write=true создаёт новый каталог path и data.bin; существующий путь не изменяется.
	static int run(lexxos::os::AppContext& context, std::string_view path, bool write,
	    std::uint32_t bytes, std::uint32_t clusterBytes, std::uint32_t delayMs,
	    std::span<char> buffer);
};

} // namespace lexxos::apps::storage_programs::detail
