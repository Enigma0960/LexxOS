#include <stdint.h>

uint32_t SystemCoreClock = 168000000U;

#define FLASH_BASE 0x40023C00U
#define RCC_BASE 0x40023800U
#define PWR_BASE 0x40007000U

#define FLASH_ACR (*(volatile uint32_t*) (FLASH_BASE + 0x00U))
#define RCC_CR (*(volatile uint32_t*) (RCC_BASE + 0x00U))
#define RCC_PLLCFGR (*(volatile uint32_t*) (RCC_BASE + 0x04U))
#define RCC_CFGR (*(volatile uint32_t*) (RCC_BASE + 0x08U))
#define RCC_APB1ENR (*(volatile uint32_t*) (RCC_BASE + 0x40U))
#define PWR_CR (*(volatile uint32_t*) (PWR_BASE + 0x00U))

#define RCC_CR_HSEON (1U << 16U)
#define RCC_CR_HSERDY (1U << 17U)
#define RCC_CR_PLLON (1U << 24U)
#define RCC_CR_PLLRDY (1U << 25U)

#define RCC_APB1ENR_PWREN (1U << 28U)

static void wait_until_set(uint32_t volatile* register_address, uint32_t mask) {
	while ((*register_address & mask) == 0U) {
	}
}

void SystemInit(void) {
	RCC_CR |= RCC_CR_HSEON;
	wait_until_set(&RCC_CR, RCC_CR_HSERDY);

	RCC_APB1ENR |= RCC_APB1ENR_PWREN;
	PWR_CR |= (1U << 14U);

	FLASH_ACR = (5U << 0U) | (1U << 8U) | (1U << 9U) | (1U << 10U);

	RCC_PLLCFGR = (8U << 0U) /* PLLM: 8 MHz HSE / 8 = 1 MHz */
	    | (336U << 6U) /* PLLN: 1 MHz * 336 = 336 MHz */
	    | (0U << 16U) /* PLLP: /2 = 168 MHz */
	    | (1U << 22U) /* PLL source: HSE */
	    | (7U << 24U); /* PLLQ: 48 MHz domain */

	RCC_CR |= RCC_CR_PLLON;
	wait_until_set(&RCC_CR, RCC_CR_PLLRDY);

	RCC_CFGR = (0U << 4U) /* AHB: 168 MHz */
	    | (5U << 10U) /* APB1: 42 MHz */
	    | (4U << 13U); /* APB2: 84 MHz */

	RCC_CFGR = (RCC_CFGR & ~0x3U) | 0x2U;
	while (((RCC_CFGR >> 2U) & 0x3U) != 0x2U) {
	}

	SystemCoreClock = 168000000U;
}
