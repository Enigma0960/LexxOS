#include <cstddef>
#include <cstdint>

#include <hardware/sync.h>

#if defined(PICO_RP2350) && PICO_RP2350

extern "C" {
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
}

namespace {
using ExceptionHandler = void (*)();
constexpr std::uintptr_t scbVtorAddress = 0xE000ED08U;
constexpr std::size_t svcVectorIndex = 11;
constexpr std::size_t pendSvVectorIndex = 14;
constexpr std::size_t sysTickVectorIndex = 15;

ExceptionHandler* activeVectorTable() {
	auto const tableAddress = *reinterpret_cast<std::uintptr_t const volatile*>(scbVtorAddress);
	return reinterpret_cast<ExceptionHandler*>(tableAddress);
}

void installExceptionHandler(std::size_t vectorIndex, ExceptionHandler handler) {
	activeVectorTable()[vectorIndex] = handler;
	__asm volatile("dmb" ::: "memory");
	__asm volatile("dsb" ::: "memory");
	__asm volatile("isb" ::: "memory");
}
} // namespace
#endif

namespace lexxos::platform::pico::detail {

void installFreeRtosExceptionHandlers() {
#if defined(PICO_RP2350) && PICO_RP2350
	// The Pico SDK owns the runtime vector table and leaves the core exceptions
	// routed to its default stubs unless an application replaces them explicitly.
	// The generic Cortex-M33 FreeRTOS port validates SVC and PendSV before it
	// starts the first task, so install the FreeRTOS handlers just before
	// vTaskStartScheduler(). Write the active vector table directly instead of
	// using exception_set_exclusive_handler(), because some Pico SDK subsystems may
	// have already replaced the compile-time default handlers during runtime init.
	// SysTick is installed as well because the port uses SysTick_Handler() as the
	// RTOS tick ISR after vPortSetupTimerInterrupt().
	auto const savedInterrupts = save_and_disable_interrupts();
	installExceptionHandler(svcVectorIndex, SVC_Handler);
	installExceptionHandler(pendSvVectorIndex, PendSV_Handler);
	installExceptionHandler(sysTickVectorIndex, SysTick_Handler);
	restore_interrupts(savedInterrupts);
#else
// RP2040 uses the Pico-specific FreeRTOS port. It does not export the generic
// Cortex-M33 SVC_Handler/PendSV_Handler/SysTick_Handler symbols.
#endif
}

} // namespace lexxos::platform::pico::detail
