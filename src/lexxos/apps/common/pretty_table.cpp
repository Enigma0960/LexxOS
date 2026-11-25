#include <lexxos/apps/common/pretty_table.h>

#include <algorithm>

#include <lexxos/os/app_context.h>

namespace lexxos::apps {

namespace {

	[[nodiscard]] std::uint16_t textWidth(std::string_view text) {
		std::uint16_t width = 0;
		for (char const character : text) {
			if (character == '\n' || character == '\r') {
				break;
			}

			if (width < 0xFFFFU) {
				++width;
			}
		}

		return width;
	}

	void writeRepeated(lexxos::os::AppContext& context, char value, std::uint16_t count) {
		for (std::uint16_t index = 0; index < count; ++index) {
			context.write(std::string_view{&value, 1});
		}
	}

	void writeSpaces(lexxos::os::AppContext& context, std::uint16_t count) {
		writeRepeated(context, ' ', count);
	}

	void writeCellText(lexxos::os::AppContext& context, std::string_view text, std::uint16_t width) {
		std::uint16_t written = 0;
		bool clipped = false;
		for (char const character : text) {
			if (character == '\n' || character == '\r') {
				break;
			}

			if (written >= width) {
				clipped = true;
				break;
			}

			if (written + 1 == width && textWidth(text) > width && width > 1) {
				context.write("~");
				++written;
				clipped = true;
				break;
			}

			context.write(std::string_view{&character, 1});
			++written;
		}

		if (!clipped && written < width) {
			writeSpaces(context, static_cast<std::uint16_t>(width - written));
		}
	}

	void writeJustified(
	    lexxos::os::AppContext& context,
	    std::string_view text,
	    std::uint16_t width,
	    PrettyTableAlign align) {
		auto const contentWidth = std::min(textWidth(text), width);
		auto const padding = static_cast<std::uint16_t>(width - contentWidth);
		std::uint16_t left = 0;
		std::uint16_t right = padding;
		if (align == PrettyTableAlign::Right) {
			left = padding;
			right = 0;
		} else if (align == PrettyTableAlign::Center) {
			left = static_cast<std::uint16_t>(padding / 2U);
			right = static_cast<std::uint16_t>(padding - left);
		}

		writeSpaces(context, left);
		writeCellText(context, text, contentWidth);
		writeSpaces(context, right);
	}

} // namespace

PrettyTable::PrettyTable(PrettyTableColumn const* columns, std::size_t columnCount)
    : _columns(columns)
    , _columnCount(std::min(columnCount, maxColumns)) {
}

void PrettyTable::setStyle(PrettyTableStyle style) {
	_style = style;
}

void PrettyTable::setMaxTableWidth(std::uint16_t width) {
	_style.maxTableWidth = width;
}

void PrettyTable::setHorizontalRules(PrettyTableRules rules) {
	_style.horizontalRules = rules;
}

void PrettyTable::setHeader(bool enabled) {
	_style.header = enabled;
}

void PrettyTable::setBorder(bool enabled) {
	_style.border = enabled;
}

void PrettyTable::print(lexxos::os::AppContext& context, RowProvider provider, void* userData) const {
	if (_columns == nullptr || _columnCount == 0 || provider == nullptr) {
		return;
	}

	std::array<std::uint16_t, maxColumns> widths{};
	PrettyTableRow row{};
	computeWidths(provider, userData, widths, row);
	fitWidths(widths);

	if (_style.border && _style.horizontalRules != PrettyTableRules::None) {
		printRule(context, widths);
	}

	if (_style.header) {
		printHeaderRow(context, widths);
		if (_style.border && (_style.horizontalRules == PrettyTableRules::Header || _style.horizontalRules == PrettyTableRules::All)) {
			printRule(context, widths);
		}
	}

	for (std::uint16_t rowIndex = 0; rowIndex < maxRowsToProbe; ++rowIndex) {
		row.reset(_columnCount);
		if (!provider(rowIndex, row, userData)) {
			break;
		}

		printRow(context, row, widths);
		if (_style.border && _style.horizontalRules == PrettyTableRules::All) {
			printRule(context, widths);
		}
	}

	if (_style.border && (_style.horizontalRules == PrettyTableRules::Frame || _style.horizontalRules == PrettyTableRules::Header)) {
		printRule(context, widths);
	}
}

std::uint16_t PrettyTable::tableWidth(std::array<std::uint16_t, maxColumns> const& widths) const {
	std::uint16_t width = 0;
	if (_style.border) {
		width = 1;
	}

	for (std::size_t column = 0; column < _columnCount; ++column) {
		width = static_cast<std::uint16_t>(width + widths[column] + _style.leftPadding + _style.rightPadding);
		if (_style.border || column + 1 < _columnCount) {
			++width;
		}
	}

	return width;
}

void PrettyTable::computeWidths(
    RowProvider provider,
    void* userData,
    std::array<std::uint16_t, maxColumns>& widths,
    PrettyTableRow& row) const {
	for (std::size_t column = 0; column < _columnCount; ++column) {
		auto const headerWidth = _style.header ? textWidth(_columns[column].name) : 0U;
		widths[column] = std::max<std::uint16_t>(headerWidth, _columns[column].minWidth);
	}

	for (std::uint16_t rowIndex = 0; rowIndex < maxRowsToProbe; ++rowIndex) {
		row.reset(_columnCount);
		if (!provider(rowIndex, row, userData)) {
			break;
		}

		for (std::size_t column = 0; column < _columnCount; ++column) {
			widths[column] = std::max(widths[column], textWidth(row.cell(column)));
		}
	}

	for (std::size_t column = 0; column < _columnCount; ++column) {
		if (_columns[column].maxWidth > 0 && widths[column] > _columns[column].maxWidth) {
			widths[column] = _columns[column].maxWidth;
		}

		if (widths[column] < _columns[column].minWidth) {
			widths[column] = _columns[column].minWidth;
		}

		if (widths[column] == 0) {
			widths[column] = 1;
		}
	}
}

void PrettyTable::fitWidths(std::array<std::uint16_t, maxColumns>& widths) const {
	if (_style.maxTableWidth == 0) {
		return;
	}

	while (tableWidth(widths) > _style.maxTableWidth) {
		std::size_t widestColumn = _columnCount;
		std::uint16_t widestWidth = 0;
		for (std::size_t column = 0; column < _columnCount; ++column) {
			auto const minWidth = std::max<std::uint16_t>(1, _columns[column].minWidth);
			if (widths[column] > minWidth && widths[column] > widestWidth) {
				widestWidth = widths[column];
				widestColumn = column;
			}
		}

		if (widestColumn >= _columnCount) {
			break;
		}

		--widths[widestColumn];
	}
}

void PrettyTable::printRule(lexxos::os::AppContext& context, std::array<std::uint16_t, maxColumns> const& widths) const {
	context.write("+");
	for (std::size_t column = 0; column < _columnCount; ++column) {
		writeRepeated(context, '-', static_cast<std::uint16_t>(widths[column] + _style.leftPadding + _style.rightPadding));
		context.write("+");
	}

	context.writeLine("");
}

void PrettyTable::printHeaderRow(
    lexxos::os::AppContext& context,
    std::array<std::uint16_t, maxColumns> const& widths) const {
	if (_style.border) {
		context.write("|");
	}

	for (std::size_t column = 0; column < _columnCount; ++column) {
		writeSpaces(context, _style.leftPadding);
		writeJustified(context, _columns[column].name, widths[column], _columns[column].align);
		writeSpaces(context, _style.rightPadding);
		if (_style.border) {
			context.write("|");
		} else if (column + 1 < _columnCount) {
			context.write(" ");
		}
	}

	context.writeLine("");
}

void PrettyTable::printRow(
    lexxos::os::AppContext& context,
    PrettyTableRow const& row,
    std::array<std::uint16_t, maxColumns> const& widths) const {
	if (_style.border) {
		context.write("|");
	}

	for (std::size_t column = 0; column < _columnCount; ++column) {
		writeSpaces(context, _style.leftPadding);
		writeJustified(context, row.cell(column), widths[column], _columns[column].align);
		writeSpaces(context, _style.rightPadding);
		if (_style.border) {
			context.write("|");
		} else if (column + 1 < _columnCount) {
			context.write(" ");
		}
	}

	context.writeLine("");
}

} // namespace lexxos::apps
