#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::apps {

class PrettyTableRow final {
public:
	static constexpr std::size_t maxColumns = 12;
	static constexpr std::size_t scratchBytesPerCell = 40;

	void reset(std::size_t columnCount);
	void set(std::size_t column, std::string_view value);
	void setUnsigned(std::size_t column, std::uint32_t value);
	void setUnsigned64(std::size_t column, std::uint64_t value);
	void setSigned(std::size_t column, int value);
	void clear(std::size_t column);
	void append(std::size_t column, std::string_view value);
	void appendUnsigned(std::size_t column, std::uint32_t value);
	void appendUnsigned64(std::size_t column, std::uint64_t value);
	void appendSigned(std::size_t column, int value);

	[[nodiscard]] std::string_view cell(std::size_t column) const;

private:
	void appendChar(std::size_t column, char value);

	std::array<std::string_view, maxColumns> _cells{};
	std::array<std::array<char, scratchBytesPerCell>, maxColumns> _scratch{};
	std::array<std::size_t, maxColumns> _scratchSize{};
	std::size_t _columnCount{0};
};

} // namespace lexxos::apps
