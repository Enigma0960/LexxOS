#include <assert.h>
#include <stdlib.h>

#include "../picocalc_splash.c"

static ulong ticks;
static unsigned int gpio, command_byte, pixel_bytes, lit_bytes;
static unsigned int enabled, selected;
static bool stuck, missing, bad_clock, drain;

ulong get_timer(ulong base) {
	return ticks++ - base;
}

void mdelay(unsigned long ms) {
	ticks += ms;
}

unsigned int readl(void const volatile* address) {
	ulong a = (ulong) address;

	if (a == SPI + 0x48) {
		return missing ? 0 : 0x1234;
	}

	if (a == CRU + 0x388) {
		return bad_clock ? 0x10 : 0;
	}

	if (a == SPI + 0x1C) {
		return stuck ? 32 : 0;
	}

	assert(a == SPI + 0x24);
	return drain ? 1 : 0;
}

void writel(unsigned int value, void volatile* address) {
	ulong a = (ulong) address;

	/* Reject any access outside the explicitly owned register ranges. */
	assert(a == GPIO || a == GPIO + 8 || a == IOC || a == IOC + 4 || a == CRU + 0x388 || a == CRU + 0x830 || a == RMIO + 0x98 || a == RMIO + 0x9C || a == 0xFF9B0800UL || (a >= SPI && a <= SPI + 0x400));
	if (a == GPIO) {
		gpio = (gpio & ~(value >> 16)) | (value & (value >> 16));
	}

	if (a == CRU + 0x388) {
		assert(value == (0x3F0U << 16));
	}

	if (a == SPI + 8) {
		enabled = value;
	}

	if (a == SPI + 12) {
		selected = value;
	}

	if (a == SPI + 0x400) {
		assert(!(gpio & BIT(CS)));
		if (!(gpio & BIT(DC))) {
			command_byte = value;
		} else if (command_byte == 0x2C) {
			assert(value == 0 || value == 0xFC);
			pixel_bytes++;
			lit_bytes += value != 0;
		}
	}
}

int main(int argc, char** argv) {
	int ret;

	assert(argc == 2);
	stuck = !strcmp(argv[1], "timeout");
	drain = !strcmp(argv[1], "drain");
	missing = !strcmp(argv[1], "missing");
	bad_clock = !strcmp(argv[1], "clock");
	ret = picocalc_splash_show();
	assert(gpio & BIT(CS));
	assert(!enabled && !selected);
	if (stuck || missing || bad_clock || drain) {
		assert(ret < 0);
		assert(!pixel_bytes);
		assert(ticks < 2000);
	} else {
		assert(!ret);
		assert(pixel_bytes == 320 * 320 * 3);
		assert(lit_bytes > 0 && lit_bytes < pixel_bytes);
		assert(command_byte == 0x29);
	}

	puts("PASS");
	return 0;
}
