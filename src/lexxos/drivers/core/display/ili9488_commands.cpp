#include <lexxos/drivers/core/display/ili9488_commands.h>

namespace lexxos::drivers::core {

namespace {

	void encode(std::uint8_t* bytes, unsigned value) {
		bytes[0] = static_cast<std::uint8_t>(value >> 8U);
		bytes[1] = static_cast<std::uint8_t>(value);
	}

	bool valid(Ili9488PowerMode mode) {
		return mode == Ili9488PowerMode::On || mode == Ili9488PowerMode::Off || mode == Ili9488PowerMode::Sleep;
	}

} // namespace

bool setIli9488Window(Ili9488CommandTransport const& transport, std::uint16_t x0, std::uint16_t y0, std::uint16_t x1, std::uint16_t y1) {
	if (!transport.write || x0 > x1 || y0 > y1) {
		return false;
	}

	std::uint8_t columns[4], rows[4];
	encode(columns, x0);
	encode(columns + 2, x1);
	encode(rows, y0);
	encode(rows + 2, y1);
	return transport.write(transport.context, 0x2A, columns, 4)
	    && transport.write(transport.context, 0x2B, rows, 4)
	    && transport.write(transport.context, 0x2C, nullptr, 0);
}

bool configureIli9488Scroll(Ili9488CommandTransport const& transport, int top, int area, int bottom) {
	if (!transport.write || top < 0 || area <= 0 || bottom < 0 || static_cast<std::int64_t>(top) + area + bottom > 65535) {
		return false;
	}

	std::uint8_t data[6];
	encode(data, top);
	encode(data + 2, area);
	encode(data + 4, bottom);
	return transport.write(transport.context, 0x33, data, 6);
}

bool setIli9488ScrollStart(Ili9488CommandTransport const& transport, int address) {
	if (!transport.write || address < 0 || address > 65535) {
		return false;
	}

	std::uint8_t data[2];
	encode(data, address);
	return transport.write(transport.context, 0x37, data, 2);
}

bool setIli9488Power(Ili9488CommandTransport const& transport, Ili9488PowerMode previous, Ili9488PowerMode next) {
	if (!transport.write || !transport.sleepMs || !valid(previous) || !valid(next)) {
		return false;
	}

	if (previous == next) {
		return true;
	}

	auto command = [&](std::uint8_t value, unsigned delay) {
		if (!transport.write(transport.context, value, nullptr, 0)) {
			return false;
		}

		if (delay) {
			transport.sleepMs(transport.context, delay);
		}

		return true;
	};
	if (next == Ili9488PowerMode::Sleep) {
		return command(0x28, 20) && command(0x10, 120);
	}

	if (previous == Ili9488PowerMode::Sleep && !command(0x11, 120)) {
		return false;
	}

	return next == Ili9488PowerMode::On ? command(0x29, 20) : command(0x28, 0);
}

} // namespace lexxos::drivers::core
