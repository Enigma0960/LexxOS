#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/pretty_table_row.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps {

enum class PrettyTableAlign : std::uint8_t {
	Left,
	Center,
	Right,
};

enum class PrettyTableRules : std::uint8_t {
	Frame,
	Header,
	All,
	None,
};

struct PrettyTableColumn {
	std::string_view name{};
	PrettyTableAlign align{PrettyTableAlign::Left};
	std::uint8_t minWidth{0};
	std::uint8_t maxWidth{0};
};

struct PrettyTableStyle {
	bool border{true};
	bool header{true};
	PrettyTableRules horizontalRules{PrettyTableRules::Header};
	std::uint8_t leftPadding{1};
	std::uint8_t rightPadding{1};
	std::uint16_t maxTableWidth{100};
};

class PrettyTable final {
public:
	using RowProvider = bool (*)(std::size_t rowIndex, PrettyTableRow& row, void* userData);

	PrettyTable(PrettyTableColumn const* columns, std::size_t columnCount);

	void setStyle(PrettyTableStyle style);
	void setMaxTableWidth(std::uint16_t width);
	void setHorizontalRules(PrettyTableRules rules);
	void setHeader(bool enabled);
	void setBorder(bool enabled);

	void print(lexxos::os::AppContext& context, RowProvider provider, void* userData) const;

private:
	static constexpr std::size_t maxColumns = PrettyTableRow::maxColumns;
	static constexpr std::uint16_t maxRowsToProbe = 512;

	[[nodiscard]] std::uint16_t tableWidth(std::array<std::uint16_t, maxColumns> const& widths) const;
	void computeWidths(
	    RowProvider provider,
	    void* userData,
	    std::array<std::uint16_t, maxColumns>& widths,
	    PrettyTableRow& row) const;
	void fitWidths(std::array<std::uint16_t, maxColumns>& widths) const;
	void printRule(lexxos::os::AppContext& context, std::array<std::uint16_t, maxColumns> const& widths) const;
	void printHeaderRow(lexxos::os::AppContext& context, std::array<std::uint16_t, maxColumns> const& widths) const;
	void printRow(
	    lexxos::os::AppContext& context,
	    PrettyTableRow const& row,
	    std::array<std::uint16_t, maxColumns> const& widths) const;

	PrettyTableColumn const* _columns{nullptr};
	std::size_t _columnCount{0};
	PrettyTableStyle _style{};
};

} // namespace lexxos::apps
