#include <stdint.h>

#define PERIPH_BASE 0x40000000U
#define AHB1_BASE (PERIPH_BASE + 0x00020000U)
#define APB2_BASE (PERIPH_BASE + 0x00010000U)

#define GPIOA_BASE (AHB1_BASE + 0x0000U)
#define GPIOC_BASE (AHB1_BASE + 0x0800U)
#define RCC_BASE (AHB1_BASE + 0x3800U)
#define USART1_BASE (APB2_BASE + 0x1000U)

#define REG32(address) (*(volatile uint32_t*) (address))

#define RCC_AHB1ENR REG32(RCC_BASE + 0x30U)
#define RCC_APB2ENR REG32(RCC_BASE + 0x44U)

#define GPIO_MODER(base) REG32((base) + 0x00U)
#define GPIO_OTYPER(base) REG32((base) + 0x04U)
#define GPIO_OSPEEDR(base) REG32((base) + 0x08U)
#define GPIO_PUPDR(base) REG32((base) + 0x0CU)
#define GPIO_IDR(base) REG32((base) + 0x10U)
#define GPIO_BSRR(base) REG32((base) + 0x18U)
#define GPIO_AFRH(base) REG32((base) + 0x24U)

#define USART_SR REG32(USART1_BASE + 0x00U)
#define USART_DR REG32(USART1_BASE + 0x04U)
#define USART_BRR REG32(USART1_BASE + 0x08U)
#define USART_CR1 REG32(USART1_BASE + 0x0CU)
#define USART_CR2 REG32(USART1_BASE + 0x10U)
#define USART_CR3 REG32(USART1_BASE + 0x14U)

#define RCC_AHB1ENR_GPIOAEN (1U << 0U)
#define RCC_AHB1ENR_GPIOCEN (1U << 2U)
#define RCC_APB2ENR_USART1EN (1U << 4U)

#define USART_SR_RXNE (1U << 5U)
#define USART_SR_TXE (1U << 7U)
#define USART_CR1_RE (1U << 2U)
#define USART_CR1_TE (1U << 3U)
#define USART_CR1_UE (1U << 13U)

static void delay(uint32_t volatile count) {
	while (count-- > 0U) {
		__asm volatile("nop");
	}
}

static void uart_write_char(char value) {
	while ((USART_SR & USART_SR_TXE) == 0U) {
	}
	USART_DR = (uint32_t) value;
}

static void uart_write(char const* text) {
	while (*text != '\0') {
		if (*text == '\n') {
			uart_write_char('\r');
		}
		uart_write_char(*text++);
	}
}

static void uart_write_hex_digit(uint32_t value) {
	value &= 0xFU;
	uart_write_char((char) (value < 10U ? ('0' + value) : ('A' + value - 10U)));
}

static void uart_write_hex32(uint32_t value) {
	uart_write("0x");
	for (int shift = 28; shift >= 0; shift -= 4) {
		uart_write_hex_digit(value >> (uint32_t) shift);
	}
}

static int uart_try_read_char(char* value) {
	if ((USART_SR & USART_SR_RXNE) == 0U) {
		return 0;
	}

	*value = (char) (USART_DR & 0xFFU);
	return 1;
}

static void gpio_configure_status_led(void) {
	GPIO_MODER(GPIOC_BASE) = (GPIO_MODER(GPIOC_BASE) & ~(3U << (0U * 2U))) | (1U << (0U * 2U));
	GPIO_OTYPER(GPIOC_BASE) &= ~(1U << 0U);
	GPIO_OSPEEDR(GPIOC_BASE) |= (2U << (0U * 2U));
}

static void status_led_set(int enabled) {
	if (enabled) {
		GPIO_BSRR(GPIOC_BASE) = (1U << (0U + 16U));
	} else {
		GPIO_BSRR(GPIOC_BASE) = (1U << 0U);
	}
}

static void uart1_configure_pa9_pa10(void) {
	RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
	RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

	(void) RCC_AHB1ENR;
	(void) RCC_APB2ENR;

	GPIO_MODER(GPIOA_BASE) = (GPIO_MODER(GPIOA_BASE) & ~((3U << (9U * 2U)) | (3U << (10U * 2U))))
	    | (2U << (9U * 2U))
	    | (2U << (10U * 2U));
	GPIO_OTYPER(GPIOA_BASE) &= ~((1U << 9U) | (1U << 10U));
	GPIO_OSPEEDR(GPIOA_BASE) |= (2U << (9U * 2U)) | (2U << (10U * 2U));
	GPIO_PUPDR(GPIOA_BASE) = (GPIO_PUPDR(GPIOA_BASE) & ~((3U << (9U * 2U)) | (3U << (10U * 2U))))
	    | (1U << (10U * 2U));
	GPIO_AFRH(GPIOA_BASE) = (GPIO_AFRH(GPIOA_BASE) & ~((0xFU << 4U) | (0xFU << 8U)))
	    | (7U << 4U)
	    | (7U << 8U);

	USART_CR1 = 0U;
	USART_CR2 = 0U;
	USART_CR3 = 0U;

	/* USART1 is clocked from APB2. With the bring-up clock tree APB2 is 84 MHz. */
	USART_BRR = 84000000U / 115200U;
	USART_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

int main(void) {
	uart1_configure_pa9_pa10();
	gpio_configure_status_led();

	uart_write("\n");
	uart_write("LexxOS STM32F407 bring-up\n");
	uart_write("board: Euse M4 DEMO Large\n");
	uart_write("cpu: STM32F407ZGT6 Cortex-M4F\n");
	uart_write("console: USART1 PA9/PA10 115200 8N1\n");
	uart_write("flash base: ");
	uart_write_hex32(0x08000000U);
	uart_write("\n");
	uart_write("sram base:  ");
	uart_write_hex32(0x20000000U);
	uart_write("\n");
	uart_write("type characters to test UART echo\n");
	uart_write("lexx-stm32> ");

	int led_enabled = 0;
	uint32_t heartbeat = 0U;

	while (1) {
		char value = 0;
		if (uart_try_read_char(&value)) {
			if (value == '\r' || value == '\n') {
				uart_write("\nlexx-stm32> ");
			} else {
				uart_write_char(value);
			}
		}

		if ((heartbeat++ % 200000U) == 0U) {
			led_enabled = !led_enabled;
			status_led_set(led_enabled);
		}

		delay(20U);
	}
}
