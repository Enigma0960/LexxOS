#include <cstddef>
#include <new>
#include <stdint.h>

#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/runtime/selected_runtime_profile.h>

extern "C" bool rk3506_debug_uart1_initialize();
extern "C" void rk3506_debug_uart1_putc(char);
extern "C" std::size_t rk3506_debug_uart1_read_available(char*, std::size_t);
extern "C" void rk3506_debug_enable_a7_swd();
extern "C" bool rk3506_debug_swd_pins_active();
#if defined(LEXXOS_RK3506_FREERTOS)
extern "C" bool rk3506_uart_claim_rx_irq(unsigned uart);
extern "C" void rk3506_debug_uart1_set_rx_interrupt(bool enabled);
#endif

namespace {

// CPU0 only. Console routing is changed from task context with IRQs masked.
// No UART0 access is allowed after the pins are assigned to SWD.
enum class ConsoleRoute : unsigned {
	Uart0 = 0,
	Uart1 = 1,
	Mirror = 2
};

ConsoleRoute g_consoleRoute{ConsoleRoute::Uart0};

bool g_uart1_ready{};
bool g_swd_enabled{};

// A 64-byte hardware FIFO fills in less than one FreeRTOS tick at 1.5 Mbaud.
// This queue is shared by foreground polling and the UART RX IRQ on CPU0.
constexpr std::size_t kRxQueueCapacity = lexxos::runtime::standardRuntimeProfile.shell.uartRxQueueBytes;
static_assert(kRxQueueCapacity >= 64U);
char g_rxQueue[kRxQueueCapacity + 1U]{}; // One unused slot.
std::size_t g_rxHead{};
std::size_t g_rxTail{};

class LocalIrqMask final {
public:
	LocalIrqMask() {
		asm volatile("mrs %0, cpsr; cpsid i" : "=r"(_cpsr) : : "memory");
	}

	~LocalIrqMask() {
		if ((_cpsr & 0x80U) == 0U) {
			asm volatile("cpsie i" ::: "memory");
		}
	}

private:
	std::uint32_t _cpsr;
};

std::size_t nextRxIndex(std::size_t index) {
	return index + 1U == kRxQueueCapacity + 1U ? 0U : index + 1U;
}

// RK3506 UART0 debug base from the PicoCalc-Lyra U-Boot defconfig.
// 16550-compatible UART with register shift 2 (32-bit MMIO registers).
constexpr uintptr_t kUartBase = 0xFF0A0000U;
constexpr uintptr_t kRegRbrThr = 0U;
constexpr uintptr_t kRegLsr = 5U * 4U;
constexpr uint32_t kLsrDataReady = 1U << 0U;
constexpr uint32_t kLsrThrEmpty = 1U << 5U;
constexpr uint32_t kDataMarker = 0x13579BDFU;

uint32_t volatile g_dataMarker = kDataMarker;
uint32_t volatile g_bssMarker;

uint32_t readRegister(uintptr_t offset) {
	return *reinterpret_cast<uint32_t volatile*>(kUartBase + offset);
}

void writeRegister(uintptr_t offset, uint32_t value) {
	*reinterpret_cast<uint32_t volatile*>(kUartBase + offset) = value;
}

void uart0Putc(char character) {
	// Do not hang indefinitely if the firmware did not configure UART0.
	for (uint32_t attempt = 0; attempt < 1000000U; ++attempt) {
		if ((readRegister(kRegLsr) & kLsrThrEmpty) != 0U) {
			writeRegister(kRegRbrThr, static_cast<uint32_t>(static_cast<uint8_t>(character)));
			return;
		}
	}
}

bool routedToConsole(unsigned uart) {
	return uart == 0U ? g_consoleRoute != ConsoleRoute::Uart1 && !g_swd_enabled
	                  : g_consoleRoute != ConsoleRoute::Uart0 && g_uart1_ready;
}

bool readUart(unsigned uart, char& value) {
	if (uart == 1U) {
		return rk3506_debug_uart1_read_available(&value, 1U) != 0U;
	}
	if ((readRegister(kRegLsr) & kLsrDataReady) == 0U) {
		return false;
	}
	value = static_cast<char>(readRegister(kRegRbrThr) & 0xFFU);
	return true;
}

#if defined(LEXXOS_RK3506_FREERTOS)
bool g_rxInterruptsInitialized{};
bool g_rxInterruptClaimed[2]{};

void updateRxInterrupts() {
	if (!g_rxInterruptsInitialized) {
		return;
	}
	for (unsigned uart = 0; uart < 2U; ++uart) {
		auto const active = routedToConsole(uart);
		if (active && !g_rxInterruptClaimed[uart]) {
			g_rxInterruptClaimed[uart] = rk3506_uart_claim_rx_irq(uart);
		}
		if (!g_rxInterruptClaimed[uart]) {
			continue;
		}
		if (uart == 0U) {
			constexpr std::uintptr_t kInterruptEnableRegister = 4U; // DLAB is clear on the console.
			constexpr std::uintptr_t kFifoControlRegister = 8U;
			constexpr std::uint32_t receive = 1U;
			if (active) {
				// Enable the FIFO and select the quarter-full RX trigger without flushing it.
				writeRegister(kFifoControlRegister, 0x41U);
			}
			auto const ier = readRegister(kInterruptEnableRegister);
			writeRegister(kInterruptEnableRegister, active ? ier | receive : ier & ~receive);
		} else {
			rk3506_debug_uart1_set_rx_interrupt(active);
		}
	}
	asm volatile("dsb sy" ::: "memory");
}
#endif

// Caller holds LocalIrqMask. Retain UART1 input priority in mirror mode.
void captureRx() {
	for (;;) {
		auto const next = nextRxIndex(g_rxTail);
		if (next == g_rxHead) {
			return; // Do not discard unread software-queued input.
		}

		char value{};
		if (!(routedToConsole(1U) && readUart(1U, value))
		    && !(routedToConsole(0U) && readUart(0U, value))) {
			return;
		}

		g_rxQueue[g_rxTail] = value;
		g_rxTail = next;
	}
}

void putc(char character) {
	// Echoing a long pasted command must not leave RX undrained for the
	// entire duration of synchronous UART TX output.
	{
		LocalIrqMask mask;
		captureRx();
	}

	if (g_consoleRoute != ConsoleRoute::Uart1 && !g_swd_enabled) {
		uart0Putc(character);
	}

	if (g_consoleRoute != ConsoleRoute::Uart0 && g_uart1_ready) {
		rk3506_debug_uart1_putc(character);
	}
}

void write(char const* text) {
	while (*text != '\0') {
		if (*text == '\n') {
			putc('\r');
		}

		putc(*text++);
	}
}

void writeHex(uint32_t value) {
	static constexpr char digits[] = "0123456789ABCDEF";
	write("0x");
	for (int shift = 28; shift >= 0; shift -= 4) {
		putc(digits[(value >> static_cast<unsigned>(shift)) & 0x0FU]);
	}
}

void writeDecimal(uint32_t value) {
	static constexpr uint32_t divisors[] = {
	    1000000000U, 100000000U, 10000000U, 1000000U, 100000U,
	    10000U, 1000U, 100U, 10U, 1U};
	bool started = false;
	for (uint32_t divisor : divisors) {
		uint32_t digit = 0U;
		while (value >= divisor) {
			value -= divisor;
			++digit;
		}

		if (digit != 0U || started || divisor == 1U) {
			putc(static_cast<char>('0' + digit));
			started = true;
		}
	}
}

uint32_t readMidr() {
	uint32_t value;
	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(value));
	return value;
}

uint32_t readSctlr() {
	uint32_t value;
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(value));
	return value;
}

} // namespace

extern "C" void rk3506_uart_write_bytes(char const* text, std::size_t length) {
	if (!text) {
		return;
	}

	for (std::size_t index = 0; index < length; ++index) {
		if (text[index] == '\n') {
			putc('\r');
		}

		putc(text[index]);
	}
}

extern "C" void rk3506_uart_write_raw_bytes(char const* bytes, std::size_t length) {
	for (std::size_t index = 0; index < length; ++index) {
		putc(bytes[index]);
	}
}

#if defined(LEXXOS_RK3506_FREERTOS)
extern "C" bool rk3506_uart_initialize_rx_interrupts() {
	LocalIrqMask mask;
	g_rxInterruptsInitialized = true;
	updateRxInterrupts();
	return (routedToConsole(0U) && g_rxInterruptClaimed[0])
	    || (routedToConsole(1U) && g_rxInterruptClaimed[1]);
}

extern "C" void rk3506_uart_rx_interrupt(unsigned uart) {
	LocalIrqMask mask;
	char value{};
	bool const active = routedToConsole(uart);
	while (readUart(uart, value)) {
		auto const next = nextRxIndex(g_rxTail);
		if (active && next != g_rxHead) {
			g_rxQueue[g_rxTail] = value;
			g_rxTail = next;
		}
		// Drain even on overflow so a full software queue cannot livelock the IRQ.
	}
}
#endif

extern "C" std::size_t rk3506_uart_read_available(char* buffer, std::size_t capacity) {
	if (!buffer || capacity == 0U) {
		return 0U;
	}

	LocalIrqMask mask;
	captureRx();
	std::size_t count = 0U;
	while (count < capacity && g_rxHead != g_rxTail) {
		buffer[count++] = g_rxQueue[g_rxHead];
		g_rxHead = nextRxIndex(g_rxHead);
	}

	return count;
}

// 0 = UART0; 1 = UART1; 2 = mirror (UART1 input takes priority).
extern "C" bool rk3506_debug_console_select(unsigned route) {
	LocalIrqMask mask;
	if (route > 2U || (g_swd_enabled && route != 1U)) {
		return false;
	}

	if (route != 0U && !g_uart1_ready) {
		if (!rk3506_debug_uart1_initialize()) {
			return false; // Previous UART0 route remains intact.
		}

		g_uart1_ready = true;
	}

	if (g_consoleRoute != static_cast<ConsoleRoute>(route)) {
		// Old-port input must not leak into the new console route.
		g_rxHead = g_rxTail;
	}

	g_consoleRoute = static_cast<ConsoleRoute>(route);
#if defined(LEXXOS_RK3506_FREERTOS)
	updateRxInterrupts();
#endif
	return true;
}

extern "C" unsigned rk3506_debug_console_mode() {
	return static_cast<unsigned>(g_consoleRoute);
}

extern "C" bool rk3506_debug_swd_enabled() {
	return g_swd_enabled;
}

extern "C" bool rk3506_debug_switch_swd() {
#if !defined(LEXXOS_RK3506_SWD_DEBUG)
	return false; // SWD pin takeover is excluded from MMU experiment builds.
#else

	if (g_swd_enabled) {
		return true;
	}

	if (!rk3506_debug_console_select(1U)) {
		return false;
	}

	// Drain UART0 fully before its TX pad becomes SWCLK. Keep bounded.
	constexpr std::uint32_t kTxCompletelyEmpty = 1U << 6U;
	for (std::uint32_t i = 0; i < 1000000U; ++i) {
		if ((readRegister(kRegLsr) & kTxCompletelyEmpty) != 0U) {
			break;
		}
	}

	rk3506_debug_enable_a7_swd();
	g_swd_enabled = true;
	return true;
#endif
}

// Called from startup.S after .bss is cleared, before global constructors.
// It must not use heap, scheduler, interrupts, or depend on C++ constructors.
extern "C" void rk3506_debug_bootstrap_console() {
	if (!rk3506_debug_swd_pins_active()) {
		return; // Cold SD boot: leave inherited UART0 entirely unchanged.
	}

	// Pinmux survived GDB RAM overwrite; software state did not.
	// Fail CLOSED if UART1 cannot start: never drive UART0/SWD pads.
	g_swd_enabled = true;
	g_consoleRoute = ConsoleRoute::Uart1;
	g_uart1_ready = rk3506_debug_uart1_initialize();
}

// The running OS calls this BEFORE parking for a GDB RAM replacement.
// The D-cache must stay disabled. The optional Normal-NC MMU maps DDR
// identically and is switched off before a debugger overwrites the image.
extern "C" bool rk3506_debug_prepare_cpu_for_ram_reload() {
	uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));

	if ((sctlr & 0x4U) != 0U) { // SCTLR.C: never reload cached DDR.
		return false; // A mapped/cached DDR requires another reload protocol.
	}

#if defined(LEXXOS_RK3506_MMU_NORMAL_NC)
	if ((sctlr & 0x1U) != 0U) {
		// The identity map permits a safe transition back to physical
		// addresses. Stop interrupts during the memory-type transition.
		asm volatile("cpsid if" ::: "memory");
		asm volatile("dsb sy" ::: "memory");
		sctlr &= ~1U; // SCTLR.M
		asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
		asm volatile("isb sy" ::: "memory");
		uint32_t const zero = 0U;
		asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero) : "memory"); // TLBIALL
		asm volatile("dsb sy; isb sy" ::: "memory");
	}
#else
	if ((sctlr & 0x1U) != 0U) {
		return false; // Unknown mapped DDR needs a separate reload protocol.
	}
#endif

	uint32_t const uncached = sctlr & ~(1U << 12U); // SCTLR.I
	asm volatile("dsb sy" ::: "memory");
	asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(uncached) : "memory");
	uint32_t const zero = 0U;
	asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero) : "memory"); // ICIALLU
	asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero) : "memory"); // BPIALL
	asm volatile("dsb sy; isb sy" ::: "memory");
	return true;
}

extern "C" void rk3506_console_write(char const* text) {
	write(text);
}

extern "C" void rk3506_console_hex(uint32_t value) {
	writeHex(value);
}

extern "C" void rk3506_console_decimal(uint32_t value) {
	writeDecimal(value);
}

extern "C" bool rk3506_irq_timer_initialize();
extern "C" std::uint32_t rk3506_scheduler_switches();
extern "C" void rk3506_irq_timer_check(uint32_t);
extern "C" uint32_t rk3506_timer_ticks();
extern "C" uint32_t rk3506_timer_counter_frequency();
extern "C" uint32_t rk3506_timer_control();
extern "C" uint32_t rk3506_timer_last_unexpected_irq();
extern "C" void rk3506_scheduler_initialize();
extern "C" void rk3506_task_yield();
extern "C" uint32_t rk3506_scheduler_switches();

// Linker .init_array is executed by startup.S before rk3506_early_main.
namespace {

std::uint32_t volatile g_constructorMarker;

struct ConstructorProbe {
	ConstructorProbe() { g_constructorMarker = 0xB007C0DEU; }
};

ConstructorProbe g_probe;

#if defined(LEXXOS_RK3506_FREERTOS)
std::uint32_t volatile g_preemptCounterA{};
std::uint32_t volatile g_preemptCounterB{};

void busyTask(void* userData) {
	auto* const counter = static_cast<std::uint32_t volatile*>(userData);
	for (;;) {
		*counter = *counter + 1U; // No yield(): verify actual time slicing.
	}
}
#else
void testTask(void* userData) {
	auto const period = *static_cast<std::uint32_t*>(userData);
	for (;;) {
		write(period == 2000U ? "[CPU0 task A] alive; tick=" : "[CPU0 task B] alive; tick=");
		writeDecimal(rk3506_timer_ticks());
		write("\n");
		lexxos::kernel::sleepMs(period);
	}
}
#endif

void consoleTask(void*) {
	write("LexxOS kernel API ready. '?' reply, 's' status.\n");
	std::uint32_t lastSecond = 0;
	for (;;) {
		std::uint32_t const ticks = rk3506_timer_ticks();
		if (ticks - lastSecond >= 1000U) {
			lastSecond += 1000U;
			write("Uptime: ");
			writeDecimal(lastSecond / 1000U);
			write(" s; ticks: ");
			writeDecimal(ticks);
			write("\n");

#if defined(LEXXOS_RK3506_FREERTOS)
			write("Preempt counters A/B: ");
			writeDecimal(g_preemptCounterA);
			write("/");
			writeDecimal(g_preemptCounterB);
			write("\n");
#endif
		}

		char value;
		if (rk3506_uart_read_available(&value, 1) != 0U) {
			if (value == '?') {
				write("\nLexxOS ARMv7-A is alive.\n");
			} else if (value == 's' || value == 'S') {
				write("\nTicks: ");
				writeDecimal(rk3506_timer_ticks());
#if defined(LEXXOS_RK3506_FREERTOS)
				write("; scheduler: FreeRTOS");
#else
				write("; task switches: ");
				writeDecimal(rk3506_scheduler_switches());
#endif
				write("; tasks: ");
				writeDecimal(static_cast<std::uint32_t>(lexxos::kernel::taskCount()));
				write("; heap free: ");
				writeDecimal(lexxos::kernel::memorySnapshot().freeHeapBytes);
				write(" bytes\n");
			} else if (value == '\r' || value == '\n') {
				write("\n");
			} else {
				putc(value);
			}
		}

		lexxos::kernel::sleepMs(1);
	}
}

} // namespace

extern "C" [[noreturn]] void rk3506_early_main() {
	// Cold SD boot uses UART0. GDB RAM re-entry restores UART1/SWD
	// from the surviving IOC pinmux in rk3506_debug_bootstrap_console().

	write("\nLexxOS / RK3506G2 bare-metal bootstrap\n");
	write("CPU MIDR: ");
	writeHex(readMidr());
	write("\n");

	auto const sctlr = readSctlr();
	write("CPU SCTLR: ");
	writeHex(sctlr);
	write(" MMU=");
	putc((sctlr & 0x1U) != 0U ? '1' : '0');
	write(" D-cache=");
	putc((sctlr & 0x4U) != 0U ? '1' : '0');
	write(" I-cache=");
	putc((sctlr & 0x1000U) != 0U ? '1' : '0');
	write("\n");

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	write("MMU policy: managed 4 KiB BOOTRAM RX/RW-XN WBWA, DMA 0x02700000 NC, guard pages; DDR else NC/XN\n");
	if ((sctlr & 0x5U) != 0x5U) {
		write("WARNING: managed MMU/D-cache not active\n");
	}
#elif defined(LEXXOS_RK3506_MMU_NORMAL_NC)
	#if defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
	write("MMU policy: BOOTRAM 8 MiB Normal WBWA; other DDR Normal-NC; MMIO Device/XN\n");
	if ((sctlr & 0x5U) != 0x5U) {
		write("WARNING: MMU/D-cache BOOTRAM policy not active\n");
	}
	#else
	write("MMU policy: 128 MiB DDR Normal-NC, MMIO Device/XN; D-cache OFF\n");
	if ((sctlr & 0x1U) == 0U || (sctlr & 0x4U) != 0U) {
		write("WARNING: MMU Normal-NC policy not active or D-cache unexpectedly ON\n");
	}
	#endif
#else
	if ((sctlr & 0x5U) != 0U) {
		write("WARNING: unexpected MMU/D-cache state for uncached DMA\n");
	}
#endif

	write(".data: ");
	write(g_dataMarker == kDataMarker ? "OK\n" : "FAILED\n");
	write(".bss: ");
	write(g_bssMarker == 0U ? "OK\n" : "FAILED\n");
	write(".init_array: ");
	write(g_constructorMarker == 0xB007C0DEU ? "OK\n" : "FAILED\n");
	g_bssMarker = 1;
	write(g_swd_enabled ? "UART1: restored after SWD RAM re-entry\n"
	                    : "UART0: inherited from U-Boot\n");
	write("Exception vectors: installed (VBAR)\n");
	if (!rk3506_irq_timer_initialize()) {
		write("FATAL: IRQ/timer initialization failed\n");
		for (;;) {
			asm volatile("wfi");
		}
	}

#if !defined(LEXXOS_RK3506_FREERTOS)
	rk3506_irq_timer_check(500U);
#endif

	// Validate the freestanding heap before the scheduler starts.
	auto* probe = new std::uint32_t(0x12345678U);
	write("C++ new/delete: ");
	bool const heapOk = probe != nullptr && *probe == 0x12345678U;
	if (probe) {
		delete probe;
	}

	write(heapOk ? "OK\n" : "FAILED\n");
	lexxos::platform::initialize();
	write("Platform: ");
	write(lexxos::platform::services().board.current().boardName);
	write("\n");

#if defined(LEXXOS_RK3506_SHELL)
	// Entry into the actual shared LexxOS app::Runtime (not a demo shell).
	extern int rk3506_shell_run();
	int const result = rk3506_shell_run();
	write("LexxOS Runtime returned: ");
	writeDecimal(static_cast<std::uint32_t>(result));
	write("; halted\n");
	for (;;) {
		asm volatile("wfi");
	}
#endif

	using lexxos::kernel::createTask;
	using lexxos::kernel::startScheduler;
	using lexxos::kernel::TaskExecutionClass;

	bool const consoleOk = createTask(consoleTask, nullptr, {.name = "console", .stackSizeBytes = 12288, .priority = 2, .executionClass = TaskExecutionClass::System, .coreAffinity = 0});
#if defined(LEXXOS_RK3506_FREERTOS)
	bool const ok = consoleOk
	    && createTask(busyTask, (void*) &g_preemptCounterA, {.name = "cpu-a", .stackSizeBytes = 2048, .priority = 1, .executionClass = TaskExecutionClass::Background, .coreAffinity = 0})
	    && createTask(busyTask, (void*) &g_preemptCounterB, {.name = "cpu-b", .stackSizeBytes = 2048, .priority = 1, .executionClass = TaskExecutionClass::Background, .coreAffinity = 0});
#else
	static std::uint32_t periodA = 2000U;
	static std::uint32_t periodB = 3000U;
	bool const ok = consoleOk
	    && createTask(testTask, &periodA, {.name = "heartbeat-a", .stackSizeBytes = 2048, .priority = 1, .executionClass = TaskExecutionClass::Background, .coreAffinity = 0})
	    && createTask(testTask, &periodB, {.name = "heartbeat-b", .stackSizeBytes = 2048, .priority = 1, .executionClass = TaskExecutionClass::Background, .coreAffinity = 0});
#endif

	if (!ok) {
		write("FATAL: could not allocate kernel tasks\n");
		for (;;) {
			asm volatile("wfi");
		}
	}

	startScheduler();
	write("All tasks exited; halted\n");
	for (;;) {
		asm volatile("wfi");
	}
}
