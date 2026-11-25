#include <lexxos/apps/common/pretty_table_row.h>

#include <algorithm>

namespace lexxos::apps {

namespace {

	void appendUnsignedToRow(PrettyTableRow& row, std::size_t column, std::uint64_t value) {
		char buffer[32]{};
		std::size_t position = sizeof(buffer);
		do {
			--position;
			buffer[position] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && position > 0);

		row.append(column, std::string_view{buffer + position, sizeof(buffer) - position});
	}

} // namespace

void PrettyTableRow::reset(std::size_t columnCount) {
	_columnCount = std::min(columnCount, maxColumns);
	for (std::size_t index = 0; index < maxColumns; ++index) {
		_cells[index] = {};
		_scratchSize[index] = 0;
		_scratch[index][0] = '\0';
	}
}

void PrettyTableRow::set(std::size_t column, std::string_view value) {
	if (column >= _columnCount) {
		return;
	}
	clear(column);
	append(column, value);
}

void PrettyTableRow::setUnsigned(std::size_t column, std::uint32_t value) {
	clear(column);
	appendUnsigned(column, value);
}

void PrettyTableRow::setUnsigned64(std::size_t column, std::uint64_t value) {
	clear(column);
	appendUnsigned64(column, value);
}

void PrettyTableRow::setSigned(std::size_t column, int value) {
	clear(column);
	appendSigned(column, value);
}

void PrettyTableRow::clear(std::size_t column) {
	if (column >= _columnCount) {
		return;
	}

	_scratchSize[column] = 0;
	_scratch[column][0] = '\0';
	_cells[column] = std::string_view{_scratch[column].data(), 0};
}

void PrettyTableRow::append(std::size_t column, std::string_view value) {
	if (column >= _columnCount) {
		return;
	}

	if (_cells[column].data() != _scratch[column].data()) {
		clear(column);
	}

	for (char const character : value) {
		appendChar(column, character);
	}

	_cells[column] = std::string_view{_scratch[column].data(), _scratchSize[column]};
}

void PrettyTableRow::appendUnsigned(std::size_t column, std::uint32_t value) {
	appendUnsigned64(column, value);
}

void PrettyTableRow::appendUnsigned64(std::size_t column, std::uint64_t value) {
	appendUnsignedToRow(*this, column, value);
}

void PrettyTableRow::appendSigned(std::size_t column, int value) {
	if (value < 0) {
		appendChar(column, '-');
		appendUnsigned64(column, static_cast<std::uint64_t>(-(value + 1)) + 1U);
		return;
	}

	appendUnsigned64(column, static_cast<std::uint32_t>(value));
}

std::string_view PrettyTableRow::cell(std::size_t column) const {
	return column < _columnCount ? _cells[column] : std::string_view{};
}

void PrettyTableRow::appendChar(std::size_t column, char value) {
	if (column >= _columnCount) {
		return;
	}

	if (_scratchSize[column] + 1 >= scratchBytesPerCell) {
		if (scratchBytesPerCell > 1) {
			_scratch[column][scratchBytesPerCell - 2] = '~';
			_scratchSize[column] = scratchBytesPerCell - 1;
			_scratch[column][_scratchSize[column]] = '\0';
		}

		return;
	}

	_scratch[column][_scratchSize[column]] = value;
	++_scratchSize[column];
	_scratch[column][_scratchSize[column]] = '\0';
}

} // namespace lexxos::apps
