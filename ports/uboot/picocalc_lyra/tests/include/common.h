#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long ulong;
typedef uint8_t u8;
typedef uint32_t u32;
#define BIT(n) (1U << (n))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define __iomem
ulong get_timer(ulong base);
void mdelay(unsigned long ms);
