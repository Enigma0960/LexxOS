#include <lexx/lexx.h>

namespace {

void clear_event(lexx::KeyEvent& event) {
	event.type = 0;
	event.modifiers = 0;
	event.character = 0;
	event.scan_code = 0;
	event.key_code = 0;
	event.reserved = 0;
	event.timestamp_ms = 0;
}

void write_hex_nibble(lexx::uint8_t value) {
	char const text[] = {static_cast<char>(value < 10 ? ('0' + value) : ('a' + value - 10)), '\0'};
	lexx::write(text);
}

void write_hex8(lexx::uint8_t value) {
	write_hex_nibble(static_cast<lexx::uint8_t>((value >> 4U) & 0x0FU));
	write_hex_nibble(static_cast<lexx::uint8_t>(value & 0x0FU));
}

void write_hex16(lexx::uint16_t value) {
	write_hex8(static_cast<lexx::uint8_t>((value >> 8U) & 0xFFU));
	write_hex8(static_cast<lexx::uint8_t>(value & 0xFFU));
}

void write_hex32(lexx::uint32_t value) {
	write_hex16(static_cast<lexx::uint16_t>((value >> 16U) & 0xFFFFU));
	write_hex16(static_cast<lexx::uint16_t>(value & 0xFFFFU));
}

void write_small_decimal(int value) {
	if (value < 0) {
		lexx::write("-");
		value = -value;
	}
	if (value >= 100) {
		lexx::write(">=100");
		return;
	}
	char text[3]{};
	int length = 0;
	if (value >= 10) {
		int tens = 0;
		while (value >= 10) {
			value -= 10;
			++tens;
		}
		text[length++] = static_cast<char>('0' + tens);
	}
	text[length++] = static_cast<char>('0' + value);
	text[length] = '\0';
	lexx::write(text);
}

void write_arguments() {
	auto const count = lexx::argc();
	lexx::write("argc=");
	write_small_decimal(count);
	lexx::write("\n");
	for (int index = 0; index < count; ++index) {
		lexx::write("argv[");
		write_small_decimal(index);
		lexx::write("]=");
		auto const value = lexx::argument(static_cast<lexx::size_t>(index));
		lexx::write(value != nullptr ? value : "<null>");
		lexx::write("\n");
	}
}

void write_key_event(lexx::KeyEvent const& event) {
	lexx::write("key event: type=0x");
	write_hex8(event.type);
	lexx::write(" key=0x");
	write_hex16(event.key_code);
	lexx::write(" char=");
	if (event.character >= 32 && event.character < 127) {
		char text[] = {static_cast<char>(event.character), '\0'};
		lexx::write(text);
	} else {
		lexx::write("0x");
		write_hex8(event.character);
	}
	lexx::write(" modifiers=0x");
	write_hex8(event.modifiers);
	lexx::write(" time=0x");
	write_hex32(event.timestamp_ms);
	lexx::write("\n");
}

int demo_main() {
	lexx::write("LexxOS external RV32I demo\n");
	write_arguments();
	lexx::write("Press keys to see events. Press q to exit. Ctrl+C or stop <id> cancels the program.\n");
	lexx::write("demo: waiting for keyboard input...\n");

	while (true) {
		lexx::KeyEvent event;
		clear_event(event);
		if (lexx::poll_key(event)) {
			write_key_event(event);
			if (event.type == static_cast<lexx::uint8_t>(lexx::KeyEventType::KeyDown)
			    && (event.character == 'q' || event.character == 'Q')) {
				lexx::write("demo: normal exit\n");
				return 0;
			}
		}
		lexx::yield();
	}
}

} // namespace

extern "C" [[noreturn]] void _start() {
	lexx::exit(demo_main());
}
