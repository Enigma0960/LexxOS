#include <lexxos/apps/detail/i2c_program_support.h>

#include <cstddef>

#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::i2c_programs::detail {

namespace {

	char hexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
	}

	void writeHex2(lexxos::os::AppContext& context, std::uint8_t value) {
		char buffer[3]{
		    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
		    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
		    '\0',
		};
		context.write(buffer);
	}

	constexpr lexxos::apps::PrettyTableColumn i2cBusColumns[] = {
	    {       .name = "name", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 16},
	    {      .name = "state", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5,  .maxWidth = 9},
	    {       .name = "lock", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4,  .maxWidth = 4},
	    {   .name = "priority", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 8, .maxWidth = 11},
	    {      .name = "owner", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5, .maxWidth = 16},
	    {.name = "description", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 11, .maxWidth = 36},
	};

	bool i2cBusTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto& devices = *static_cast<lexxos::devices::DeviceManager*>(userData);
		if (rowIndex >= devices.i2cBusCount()) {
			return false;
		}
		auto const& descriptor = devices.i2cBusAt(rowIndex);
		auto const status = devices.i2cBusStatusAt(rowIndex);
		row.set(0, descriptor.name);
		row.set(1, descriptor.bus != nullptr && descriptor.bus->isReady() ? "ready" : "not-ready");
		row.set(2, status.locked ? "busy" : "free");
		row.set(3, lexxos::devices::toString(status.priority));
		row.set(4, status.owner.empty() ? "-" : status.owner);
		row.set(5, descriptor.description);
		return true;
	}

} // namespace

void writeHex8(lexxos::os::AppContext& context, std::uint8_t value) {
	char buffer[5]{
	    '0',
	    'x',
	    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
	    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
	    '\0',
	};
	context.write(buffer);
}

void writeUnsigned(lexxos::os::AppContext& context, std::uint32_t value) {
	char buffer[16]{};
	int position = static_cast<int>(sizeof(buffer)) - 1;
	buffer[position] = '\0';

	if (value == 0) {
		context.write("0");
		return;
	}

	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}

	context.write(&buffer[position]);
}

bool parseUnsigned(std::string_view text, std::uint32_t& output) {
	if (text.empty()) {
		return false;
	}

	std::uint32_t base = 10;
	std::size_t index = 0;
	if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		index = 2;
		if (index >= text.size()) {
			return false;
		}
	}

	std::uint32_t value = 0;
	for (; index < text.size(); ++index) {
		char const character = text[index];
		std::uint32_t digit = 0;
		if (character >= '0' && character <= '9') {
			digit = static_cast<std::uint32_t>(character - '0');
		} else if (character >= 'a' && character <= 'f') {
			digit = static_cast<std::uint32_t>(10 + character - 'a');
		} else if (character >= 'A' && character <= 'F') {
			digit = static_cast<std::uint32_t>(10 + character - 'A');
		} else {
			return false;
		}

		if (digit >= base) {
			return false;
		}
		value = value * base + digit;
	}

	output = value;
	return true;
}

void writeI2cBusList(lexxos::os::AppContext& context, bool errorOutput) {
	auto& devices = context.devices();
	if (!errorOutput) {
		lexxos::apps::PrettyTable table{i2cBusColumns, sizeof(i2cBusColumns) / sizeof(i2cBusColumns[0])};
		table.print(context, i2cBusTableRow, &devices);
		return;
	}

	for (std::size_t index = 0; index < devices.i2cBusCount(); ++index) {
		auto const& descriptor = devices.i2cBusAt(index);
		auto const ready = descriptor.bus != nullptr && descriptor.bus->isReady() ? "ready" : "not-ready";
		auto const lockState = devices.i2cBusStatusAt(index);
		if (errorOutput) {
			context.writeError(descriptor.name);
			context.writeError(" - ");
			context.writeError(descriptor.description);
			context.writeError(" [");
			context.writeError(ready);
			context.writeError(",");
			context.writeError(lockState.locked ? "busy" : "free");
			if (lockState.locked) {
				context.writeError(",");
				context.writeError(lexxos::devices::toString(lockState.priority));
				context.writeError(",");
				context.writeError(lockState.owner.empty() ? "-" : lockState.owner);
			}
			context.writeErrorLine("]");
		} else {
			context.write(descriptor.name);
			context.write(" - ");
			context.write(descriptor.description);
			context.write(" [");
			context.write(ready);
			context.write(",");
			context.write(lockState.locked ? "busy" : "free");
			if (lockState.locked) {
				context.write(",");
				context.write(lexxos::devices::toString(lockState.priority));
				context.write(",");
				context.write(lockState.owner.empty() ? "-" : lockState.owner);
			}
			context.writeLine("]");
		}
	}
}

std::uint32_t writeI2cAddressTable(lexxos::os::AppContext& context,
    lexxos::devices::I2cBus& bus,
    std::uint8_t from,
    std::uint8_t to) {
	context.writeLine("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
	std::uint32_t found = 0;
	for (std::uint8_t row = 0; row <= 0x70; row = static_cast<std::uint8_t>(row + 0x10)) {
		writeHex2(context, row);
		context.write(":");
		for (std::uint8_t column = 0; column < 0x10; ++column) {
			auto const address = static_cast<std::uint8_t>(row + column);
			context.write(" ");
			if (address < from || address > to) {
				context.write("  ");
				continue;
			}
			if (bus.probe(address)) {
				writeHex2(context, address);
				++found;
			} else {
				context.write("--");
			}
		}
		context.writeLine("");
	}
	return found;
}

} // namespace lexxos::apps::i2c_programs::detail
