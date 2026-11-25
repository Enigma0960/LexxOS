/* SPDX-License-Identifier: GPL-2.0+ */
/* PicoCalc/Luckfox Lyra SPI0 boot splash, U-Boot proper only. */
#include <command.h>
#include <common.h>

#include <asm/io.h>
#include <linux/errno.h>

#define SPI 0xFF120000UL
#define GPIO 0xFF940000UL
#define IOC 0xFF950000UL
#define CRU 0xFF9A0000UL
#define RMIO 0xFF910000UL
#define RESET 2
#define DC 3
#define CS 4
#define SIZE 320

static void reg_write(ulong address, u32 value) {
	writel(value, (void __iomem*) address);
}

static u32 reg_read(ulong address) {
	return readl((void __iomem*) address);
}

static void pin(unsigned int n, bool high) {
	reg_write(GPIO, BIT(n + 16) | (high ? BIT(n) : 0));
}

/* All callers drain the FIFO before changing DC or CS. */
static int transmit(u8 const* data, unsigned int count) {
	ulong start = get_timer(0);

	while (count) {
		if (reg_read(SPI + 0x1C) < 28) {
			reg_write(SPI + 0x400, *data++);
			count--;
			start = get_timer(0);
		} else if (get_timer(start) > 1000) {
			return -ETIMEDOUT;
		}
	}

	start = get_timer(0);
	while (reg_read(SPI + 0x1C) || (reg_read(SPI + 0x24) & 1)) {
		if (get_timer(start) > 1000) {
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int command(u8 cmd, u8 const* data, unsigned int count) {
	int ret;

	pin(CS, false);
	pin(DC, false);
	ret = transmit(&cmd, 1);
	if (!ret && count) {
		pin(DC, true);
		ret = transmit(data, count);
	}

	pin(CS, true);
	return ret;
}

struct panel_command {
	u8 cmd;
	u8 count;
	u8 data[16];
};

static const struct panel_command init[] = {
    {0xE0, 15,       {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F}},
    {0xE1, 16, {0x00, 0x16, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F}},
    {0xC0,  2,                                                                                     {0x17, 0x15}},
    {0xC1,  1,                                                                                           {0x41}},
    {0xC5,  3,                                                                               {0x00, 0x12, 0x80}},
    {0x36,  1,                                                                                           {0x48}},
    {0x3A,  1,                                                                                           {0x66}}, /* RGB666: three bytes per pixel. */
    {0xB0,  1,                                                                                           {0x00}},
    {0xB1,  1,                                                                                           {0xA0}},
    {0x21,  0,                                                                                              {0}},
    {0xB4,  1,                                                                                           {0x02}},
    {0xB6,  3,                                                                               {0x02, 0x02, 0x3B}},
    {0xB7,  1,                                                                                           {0xC6}},
    {0xE9,  1,                                                                                           {0x00}},
    {0xF7,  4,                                                                         {0xA9, 0x51, 0x2C, 0x82}},
};

/* Five-column glyphs, top pixel in bit 0: L E X X O S. */
static u8 const logo[] = {
    0x7F,
    0x40,
    0x40,
    0x40,
    0x40,
    0,
    0x7F,
    0x49,
    0x49,
    0x49,
    0x41,
    0,
    0x63,
    0x14,
    0x08,
    0x14,
    0x63,
    0,
    0x63,
    0x14,
    0x08,
    0x14,
    0x63,
    0,
    0x3E,
    0x41,
    0x41,
    0x41,
    0x3E,
    0,
    0x46,
    0x49,
    0x49,
    0x49,
    0x31,
};

int picocalc_splash_show(void) {
	static u8 const window[] = {0, 0, 1, 0x3F};
	u8 row[SIZE * 3];
	unsigned int i, x, y;
	u32 version;
	int ret = -ENODEV;

	/* Enable GPIO0/IOC, SPI0 clocks; hiword masks preserve other pins. */
	reg_write(0xFF9B0800UL, BIT(23) | BIT(24));
	for (i = RESET; i <= CS; i++) {
		reg_write(IOC + (i / 4) * 4, 0xFU << (16 + (i % 4) * 4));
		pin(i, true);
		reg_write(GPIO + 8, BIT(i + 16) | BIT(i));
	}

	reg_write(CRU + 0x830, BIT(26) | BIT(27));
	reg_write(RMIO + 0x98, (0x7FU << 16) | 68);
	reg_write(RMIO + 0x9C, (0x7FU << 16) | 67);
	reg_write(IOC + 4, 0xFF007700);
	version = reg_read(SPI + 0x48);
	if (!version || version == 0xFFFFFFFF) {
		goto out;
	}

	reg_write(SPI + 8, 0);
	reg_write(SPI + 12, 0);
	/* OSC24M / 1 / BAUDR(2) = 12 MHz. Preserve SPI1 clock fields. */
	reg_write(CRU + 0x388, 0x3F0U << 16);
	if (reg_read(CRU + 0x388) & 0x3F0) {
		goto out;
	}

	reg_write(SPI + 0x2C, 0);
	reg_write(SPI + 0x3C, 0);
	reg_write(SPI + 0x38, 0xFFFFFFFF);
	reg_write(SPI, 1 | BIT(10) | BIT(11) | BIT(13) | BIT(18));
	reg_write(SPI + 4, 0xFFFF);
	reg_write(SPI + 0x10, 2);
	reg_write(SPI + 8, 1);
	reg_write(SPI + 12, 1);
	mdelay(10);
	pin(RESET, false);
	mdelay(10);
	pin(RESET, true);
	mdelay(200);
	for (i = 0; i < ARRAY_SIZE(init); i++) {
		ret = command(init[i].cmd, init[i].data, init[i].count);
		if (ret) {
			goto out;
		}
	}

	ret = command(0x11, NULL, 0);
	if (ret) {
		goto out;
	}

	mdelay(120);
	ret = command(0x2A, window, sizeof(window));
	if (ret) {
		goto out;
	}

	ret = command(0x2B, window, sizeof(window));
	if (ret) {
		goto out;
	}

	ret = command(0x2C, NULL, 0);
	if (ret) {
		goto out;
	}

	pin(CS, false);
	pin(DC, true);
	for (y = 0; y < SIZE; y++) {
		for (x = 0; x < SIZE; x++) {
			bool lit = x >= 90 && x < 230 && y >= 146 && y < 174 && (logo[(x - 90) / 4] & BIT((y - 146) / 4));

			memset(row + x * 3, lit ? 0xFC : 0, 3);
		}

		ret = transmit(row, sizeof(row));
		if (ret) {
			goto out;
		}
	}

	pin(CS, true);
	/* Enable display only after the complete frame is in panel RAM. */
	ret = command(0x29, NULL, 0);
	if (!ret) {
		mdelay(120);
	}

out:
	pin(CS, true);
	reg_write(SPI + 12, 0);
	reg_write(SPI + 8, 0);
	if (ret) {
		printf("PicoCalc splash failed: %d (continuing boot)\n", ret);
	}

	return ret;
}

static int do_picocalc_splash(cmd_tbl_t* cmdtp, int flag, int argc,
    char* const argv[]) {
	return picocalc_splash_show() ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(picocalc_splash, 1, 0, do_picocalc_splash,
    "display PicoCalc/Lyra boot logo", "");
