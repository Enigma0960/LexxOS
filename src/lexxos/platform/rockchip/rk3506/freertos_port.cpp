/* RK3506 Cortex-A7 integration for the FreeRTOS GCC ARM_CA9 port.
 * FreeRTOS IRQ entry owns GICC_IAR/GICC_EOIR and context switching.
 */
#include <stdint.h>

extern "C" void rk3506_irq_dispatch_ack(uint32_t acknowledge);
extern "C" void rk3506_console_write(char const*);
extern "C" void rk3506_console_decimal(uint32_t);

extern "C" void vApplicationIRQHandler(uint32_t acknowledge) {
	rk3506_irq_dispatch_ack(acknowledge);
}

extern "C" void rk3506_freertos_assert_failure(char const* file, unsigned line) {
	asm volatile("cpsid if" ::: "memory");

	rk3506_console_write("\nFATAL: FreeRTOS assert at ");
	rk3506_console_write(file);
	rk3506_console_write(":");
	rk3506_console_decimal(line);
	rk3506_console_write("\n");

	for (;;) {
		asm volatile("wfi");
	}
}
