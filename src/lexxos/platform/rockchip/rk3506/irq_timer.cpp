#include <stdint.h>

#include <lexxos/feature.h>
#include <lexxos/platform/rockchip/rk3506/dma_irq_clients.h>

#if defined(LEXXOS_RK3506_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>

	#include <lexxos/kernel/synchronization.h>
	#include <lexxos/platform/rockchip/rk3506/irq_tick_probe.h>

extern "C" void FreeRTOS_Tick_Handler();
extern "C" void rk3506_uart_rx_interrupt(unsigned uart);
#endif

extern "C" bool rk3506_dma_irq_handle(uint32_t id);
#if FEATURE_ENABLED(DISPLAY) && defined(LEXXOS_RK3506_SHELL)
extern "C" bool rk3506_lcd_dma_irq_handle(uint32_t id);
#endif
static bool dispatchDmaInterrupt(uint32_t id) {
	bool const sd = rk3506_dma_irq_handle(id);
#if FEATURE_ENABLED(DISPLAY) && defined(LEXXOS_RK3506_SHELL)
	bool const lcd = rk3506_lcd_dma_irq_handle(id);
	return sd || lcd;
#else
	return sd;
#endif
}

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	#include <lexxos/platform/rockchip/rk3506/gic_irq_controller.h>
#endif

namespace {

constexpr uintptr_t kGicDistributor = 0xFF581000U;
constexpr uintptr_t kGicCpuInterface = 0xFF582000U;
constexpr uint32_t kVirtualTimerPpi = 27U;
constexpr uint32_t kSpuriousIrq = 1023U;
constexpr uint32_t kExpectedTimerHz = 24000000U;

constexpr uintptr_t kGicdCtlr = 0x000U;
constexpr uintptr_t kGicdTyper = 0x004U;
constexpr uintptr_t kGicdIsenabler0 = 0x100U;
constexpr uintptr_t kGicdIpriorityr = 0x400U;
constexpr uintptr_t kGiccCtlr = 0x000U;
constexpr uintptr_t kGiccPmr = 0x004U;
constexpr uintptr_t kGiccIar = 0x00CU;
constexpr uintptr_t kGiccEoir = 0x010U;

uint32_t volatile g_ticks;
uint32_t volatile g_unexpectedIrq;
uint32_t g_tickReload;

#if defined(LEXXOS_RK3506_FREERTOS)
// CPU0 only. Task-context arm/disarm masks the FreeRTOS-owned PPI27.
// Never access the probe after disarm: the Event may then be destroyed.
lexxos::kernel::Event* g_tickProbeEvent{};
std::uint32_t g_tickProbeNotifications{};

// The board DTB assigns level-high GIC SPIs 34 and 35 to UART0 and UART1.
constexpr uint32_t kFirstUartRxIntId = 32U + 34U;
constexpr uint32_t kUartRxInterruptCount = 2U;
bool g_uartRxIrqClaimed[kUartRxInterruptCount]{};
#endif

// Every GICv2 INTID is counted only when actually ACKed by this CPU0.
// This is NOT a measure of peripheral events or Secure-world deliveries.
constexpr uint32_t kMaxGicIntids = 1020U;
uint32_t volatile g_irqDeliveries[kMaxGicIntids]{};

uint32_t readMmio(uintptr_t address) {
	return *reinterpret_cast<uint32_t volatile*>(address);
}

void writeMmio(uintptr_t address, uint32_t value) {
	*reinterpret_cast<uint32_t volatile*>(address) = value;
}

void dsb() {
	asm volatile("dsb sy" ::: "memory");
}

void isb() {
	asm volatile("isb sy" ::: "memory");
}

#if defined(LEXXOS_RK3506_FREERTOS)
bool dispatchUartRxInterrupt(uint32_t id) {
	if (id < kFirstUartRxIntId || id >= kFirstUartRxIntId + kUartRxInterruptCount) {
		return false;
	}
	auto const uart = id - kFirstUartRxIntId;
	if (!g_uartRxIrqClaimed[uart]) {
		return false;
	}
	rk3506_uart_rx_interrupt(uart);
	dsb();
	return true;
}
#endif

uint32_t readCounterFrequency() {
	uint32_t value;
	asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(value));
	return value;
}

uint64_t readVirtualCounter() {
	uint32_t low;
	uint32_t high;
	asm volatile("mrrc p15, 1, %0, %1, c14" : "=r"(low), "=r"(high));
	return (static_cast<uint64_t>(high) << 32U) | low;
}

uint32_t readVirtualTimerControl() {
	uint32_t value;
	asm volatile("mrc p15, 0, %0, c14, c3, 1" : "=r"(value));
	return value;
}

void setVirtualTimerControl(uint32_t value) {
	asm volatile("mcr p15, 0, %0, c14, c3, 1" : : "r"(value) : "memory");
	isb();
}

void setVirtualTimerValue(uint32_t ticks) {
	asm volatile("mcr p15, 0, %0, c14, c3, 0" : : "r"(ticks) : "memory");
	isb();
}

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
class NativeGicRegisters final : public lexxos::platform::rk3506::GicRegisterAccess {
public:
	[[nodiscard]] uint32_t read(uintptr_t address) const noexcept override { return readMmio(address); }

	void write(uintptr_t address, uint32_t value) noexcept override {
		writeMmio(address, value);
		dsb();
	}

	[[nodiscard]] uintptr_t saveAndDisableIrq() noexcept override {
		uint32_t cpsr;
		asm volatile("mrs %0, cpsr; cpsid i" : "=r"(cpsr) : : "memory");
		return cpsr;
	}

	void restoreIrq(uintptr_t cpsr) noexcept override {
		dsb();
		if ((cpsr & 0x80U) == 0U) {
			asm volatile("cpsie i" ::: "memory");
		}
	}
};

NativeGicRegisters g_nativeGicRegisters;

lexxos::platform::rk3506::GicIrqController g_gicIrqController{
    g_nativeGicRegisters,
	#if defined(LEXXOS_RK3506_MANAGED_DMA_IRQ)
    true
	#else
    false
	#endif
};

lexxos::kernel::IrqManager<2> g_irqManager{g_gicIrqController};
lexxos::kernel::IrqSubscription g_timerSubscription{};
int g_timerOwner{};

extern "C" bool rk3506_gic_irq_snapshot(
    uint32_t id, lexxos::platform::rk3506::GicIrqSnapshot* output) {
	if (output == nullptr) {
		return false;
	}

	return g_gicIrqController.snapshot({lexxos::platform::rk3506::GicIrqController::domainId, id}, *output)
	    == lexxos::kernel::IrqStatus::Ok;
}

extern "C" bool rk3506_gic_irq_bank_snapshot(
    lexxos::platform::rk3506::GicIrqBankSnapshot* output) {
	if (output == nullptr) {
		return false;
	}

	g_gicIrqController.bankSnapshot(*output);
	return true;
}

extern "C" uint32_t rk3506_gic_irq_delivery_count(uint32_t id) {
	return id < kMaxGicIntids ? g_irqDeliveries[id] : 0U;
}

	#if defined(LEXXOS_RK3506_MANAGED_DMA_IRQ)
lexxos::kernel::IrqSubscription g_dmaSubscription{};
int g_dmaOwner{};
extern "C" bool rk3506_dma_irq_handle(uint32_t id);

lexxos::kernel::IrqDisposition managedDmaInterrupt(lexxos::kernel::IrqSourceId source, void*) noexcept {
	if (source != lexxos::platform::rk3506::GicIrqController::dmaSource) {
		return lexxos::kernel::IrqDisposition::NotHandled;
	}

	return dispatchDmaInterrupt(148U)
	    ? lexxos::kernel::IrqDisposition::Handled
	    : lexxos::kernel::IrqDisposition::NotHandled;
}
	#endif

lexxos::kernel::IrqDisposition managedTimerInterrupt(lexxos::kernel::IrqSourceId source, void*) noexcept {
	if (source != lexxos::platform::rk3506::GicIrqController::timerSource) {
		return lexxos::kernel::IrqDisposition::NotHandled;
	}

	// Timer is level-triggered: move comparator before GICC_EOIR.
	setVirtualTimerValue(g_tickReload);
	g_ticks = g_ticks + 1U;
	return lexxos::kernel::IrqDisposition::Handled;
}
#endif

// Console functions live in early_uart.cpp. Never call them in the 1 ms IRQ.
extern "C" void rk3506_console_write(char const*);
extern "C" void rk3506_console_hex(uint32_t);
extern "C" void rk3506_console_decimal(uint32_t);

} // namespace

#if defined(LEXXOS_RK3506_FREERTOS)
// The UART driver calls this with IRQs masked on CPU0. It owns the UART
// interrupt for the boot session, and masks the peripheral when routed away.
extern "C" bool rk3506_uart_claim_rx_irq(unsigned uart) {
	if (uart >= kUartRxInterruptCount) {
		return false;
	}
	if (g_uartRxIrqClaimed[uart]) {
		return true;
	}
	auto const id = kFirstUartRxIntId + uart;
	auto const bit = 1U << (id % 32U);
	auto const bank = (id / 32U) * 4U;
	auto const enableRegister = kGicDistributor + kGicdIsenabler0 + bank;
	auto const trigger = readMmio(kGicDistributor + 0xC00U + (id / 16U) * 4U);
	if (id >= ((readMmio(kGicDistributor + kGicdTyper) & 31U) + 1U) * 32U
	    || (readMmio(enableRegister) & bit) != 0U
	    || (trigger & (2U << ((id % 16U) * 2U))) != 0U) {
		return false;
	}

	auto const shift = (id % 4U) * 8U;
	auto const byteMask = 0xFFU << shift;
	auto const priorityRegister = kGicDistributor + kGicdIpriorityr + (id & ~3U);
	auto const targetRegister = kGicDistributor + 0x800U + (id & ~3U);
	auto const previousPriority = readMmio(priorityRegister);
	auto const previousTarget = readMmio(targetRegister);
	auto const target = (previousTarget & byteMask) >> shift;
	if (target != 0U && target != 1U) {
		return false;
	}

	// RX only touches MMIO and its CPU0 queue. Allow it through the FreeRTOS
	// priority mask, since a 64-byte FIFO fills in under 0.5 ms at 1.5 Mbaud.
	constexpr auto priority = (configMAX_API_CALL_INTERRUPT_PRIORITY - 1U) << portPRIORITY_SHIFT;
	writeMmio(priorityRegister, (previousPriority & ~byteMask) | (priority << shift));
	writeMmio(targetRegister, (previousTarget & ~byteMask) | (1U << shift));
	dsb();
	if ((readMmio(priorityRegister) & byteMask) == (priority << shift)
	    && (readMmio(targetRegister) & byteMask) == (1U << shift)) {
		writeMmio(kGicDistributor + 0x280U + bank, bit); // ICPENDR
		writeMmio(enableRegister, bit);
		dsb();
		if ((readMmio(enableRegister) & bit) != 0U) {
			g_uartRxIrqClaimed[uart] = true;
			return true;
		}
	}
	writeMmio(priorityRegister, previousPriority);
	writeMmio(targetRegister, previousTarget);
	dsb();
	return false;
}
#endif

// The PL330 handler only touches its own INTCLR and sticky event flags.
// A false result means the level source cannot be acknowledged yet:
// mask that one GIC line BEFORE EOI, then let the foreground recover.
extern "C" bool rk3506_dma_irq_handle(uint32_t id);

// The FreeRTOS vector owns IAR and EOI. This function only services the device.
extern "C" void rk3506_irq_dispatch_ack(uint32_t ack) {
	uint32_t const id = ack & 0x3FFU;

	if (id < kMaxGicIntids) {
		g_irqDeliveries[id] = g_irqDeliveries[id] + 1U;
	}

	if (id == kVirtualTimerPpi) {
#if defined(LEXXOS_RK3506_FREERTOS)
		FreeRTOS_Tick_Handler();
#elif defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
		if (g_irqManager.dispatch(lexxos::platform::rk3506::GicIrqController::timerSource)
		    == lexxos::kernel::IrqDisposition::NotHandled) {
			// No subscriber acknowledged our level PPI: prevent an IRQ storm.
			writeMmio(kGicDistributor + 0x180U, 1U << kVirtualTimerPpi);
			g_unexpectedIrq = kVirtualTimerPpi + 1U;
			dsb();
		}
#else
		// This is a level interrupt; move the comparison point BEFORE EOI.
		setVirtualTimerValue(g_tickReload);
		g_ticks = g_ticks + 1U;
#endif
#if defined(LEXXOS_RK3506_FREERTOS)
	} else if (dispatchUartRxInterrupt(id)) {
		return;
#endif
	} else if (id == 148U || id == 149U) { // DMAC0 GIC SPI116/117 (DTB dma-controller@ff000000).

#if defined(LEXXOS_RK3506_MANAGED_DMA_IRQ)
		if (id == 148U) {
			if (!g_gicIrqController.dmaClaimed()) {
				// Unowned IRQ: never alter another driver's GIC state.
				g_unexpectedIrq = id + 1U;
				asm volatile("cpsid if" ::: "memory");
				while (true) {
					asm volatile("wfi");
				}
			}

			if (g_irqManager.dispatch(lexxos::platform::rk3506::GicIrqController::dmaSource)
			    == lexxos::kernel::IrqDisposition::NotHandled) {
				g_gicIrqController.maskUnacknowledgedDmaFromIsr();
			}
		} else if (!dispatchDmaInterrupt(id)) {
			// IRQ149 is NOT subscribed/enabled; retain emergency masking
			// until the second DMAC0 output is classified on hardware.
			writeMmio(kGicDistributor + 0x180U + (id / 32U) * 4U, 1U << (id % 32U));
		}
#else
		if (!dispatchDmaInterrupt(id)) {
			writeMmio(kGicDistributor + 0x180U + (id / 32U) * 4U, 1U << (id % 32U));
		}
#endif

		dsb(); // Ensure PL330 INTCLR reached the bus before GIC EOI.
	} else if (id != kSpuriousIrq) {
		// We haven't installed drivers for any other IRQ. Halt rather than
		// endlessly acknowledging a level interrupt whose source stays active.
		g_unexpectedIrq = id + 1U;
		asm volatile("cpsid if" ::: "memory");
		rk3506_console_write("\nUnexpected GIC IRQ ID: ");
		rk3506_console_decimal(id);
		rk3506_console_write("; halted\n");

		while (true) {
			asm volatile("wfi");
		}
	}
}

#if !defined(LEXXOS_RK3506_FREERTOS)
extern "C" void rk3506_irq_dispatch() {
	uint32_t const ack = readMmio(kGicCpuInterface + kGiccIar);
	rk3506_irq_dispatch_ack(ack);

	if ((ack & 0x3FFU) != kSpuriousIrq) {
		dsb(); // Device source must be cleared before GIC EOI.
		writeMmio(kGicCpuInterface + kGiccEoir, ack);
	}
}
#endif

static bool configureDmaIrqLine(bool enabled);
static lexxos::platform::rk3506::DmaIrqClients dmaClients{configureDmaIrqLine};

extern "C" bool rk3506_dma_irq_configure(bool enabled) {
	return dmaClients.setEnabled(0, enabled);
}

extern "C" bool rk3506_lcd_dma_irq_configure(bool enabled) {
	return dmaClients.setEnabled(1, enabled);
}

static bool configureDmaIrqLine(bool enabled) {
#if defined(LEXXOS_RK3506_MANAGED_DMA_IRQ)
	using lexxos::kernel::IrqStatus;
	if (!enabled) {
		if (!g_dmaSubscription.valid()) {
			return true;
		}

		if (g_irqManager.setEnabled(g_dmaSubscription, &g_dmaOwner, false) != IrqStatus::Ok) {
			return false;
		}

		if (g_irqManager.unsubscribe(g_dmaSubscription, &g_dmaOwner) != IrqStatus::Ok) {
			return false;
		}

		g_dmaSubscription = {};
		return true;
	}

	if (g_gicIrqController.dmaFaulted()) {
		return false; // Requires disable/detach before next registration.
	}

	if (!g_dmaSubscription.valid()) {
		lexxos::kernel::IrqRequest request{
		    .source = lexxos::platform::rk3506::GicIrqController::dmaSource,
		    .owner = &g_dmaOwner,
		    .handler = managedDmaInterrupt,
		};

		IrqStatus const status = g_irqManager.subscribe(request, g_dmaSubscription);
		if (status != IrqStatus::Ok) {
			rk3506_console_write("DMAC0 managed IRQ148 attach failed, status=");
			rk3506_console_decimal(static_cast<uint32_t>(status));
			rk3506_console_write("\n");
			return false;
		}
	}

	IrqStatus const status = g_irqManager.setEnabled(g_dmaSubscription, &g_dmaOwner, true);
	if (status != IrqStatus::Ok) {
		rk3506_console_write("DMAC0 managed IRQ148 enable failed, status=");
		rk3506_console_decimal(static_cast<uint32_t>(status));
		rk3506_console_write("\n");
		(void) configureDmaIrqLine(false);
		return false;
	}

	return true;
#else
	// Do not disable an IRQ line which this driver never acquired.
	static bool claimed{};
	if (!enabled && !claimed) {
		return true;
	}

	constexpr uint32_t id = 148U; // RK3506 DTB: GIC_SPI 116 + 32.
	constexpr uint32_t bit = 1U << (id % 32U);
	constexpr uintptr_t bankOffset = (id / 32U) * 4U;

	constexpr uint32_t targetShift = (id % 4U) * 8U;
	constexpr uintptr_t priorityRegister = kGicDistributor + 0x400U + (id & ~3U);
	constexpr uintptr_t targetRegister = kGicDistributor + 0x800U + (id & ~3U);
	constexpr uintptr_t triggerRegister = kGicDistributor + 0xC00U + (id / 16U) * 4U;

	if (enabled) {
		// Read-only diagnostic BEFORE touching GIC. In particular, never mask
		// another owner's IRQ if its security/trigger/CPU target is unsuitable.
		// GICD_IGROUPRn is RAZ/WI to Non-secure PL1: group unknown.
		uint32_t const edge = (readMmio(triggerRegister) & (2U << (2U * (id % 16U)))) != 0U;
		uint32_t const target = (readMmio(targetRegister) >> targetShift) & 0xFFU;
		uint32_t const alreadyEnabled = (readMmio(kGicDistributor + 0x100U + bankOffset) & bit) != 0U;
		uint32_t const pending = (readMmio(kGicDistributor + 0x200U + bankOffset) & bit) != 0U;

		rk3506_console_write("DMAC0 GIC IRQ148 group=unknown (Non-secure), edge/target/enabled/pending: ");
		rk3506_console_decimal(edge);
		rk3506_console_write("/");
		rk3506_console_decimal(target);
		rk3506_console_write("/");
		rk3506_console_decimal(alreadyEnabled);
		rk3506_console_write("/");
		rk3506_console_decimal(pending);
		rk3506_console_write("\n");

		if (alreadyEnabled != 0U && !claimed) {
			rk3506_console_write("DMAC0 IRQ refused: GIC IRQ148 already enabled by firmware or another owner");
			return false;
		}

		if (edge != 0U) {
			rk3506_console_write("DMAC0 IRQ refused: GIC IRQ148 configured as edge-triggered\n");
			return false;
		}

		if (target != 0U && (target & 1U) == 0U) {
			rk3506_console_write("DMAC0 IRQ refused: GIC IRQ148 routed away from CPU0\n");
			return false;
		}
	}

	// The line is ours only after the checks above. Mask, clear old GIC
	// pending state, and configure CPU0 routing before enabling it.
	writeMmio(kGicDistributor + 0x180U + bankOffset, bit); // ICENABLER
	writeMmio(kGicDistributor + 0x280U + bankOffset, bit); // ICPENDR

	uint32_t const previousPriority = enabled ? readMmio(priorityRegister) : 0U;
	uint32_t const previousTarget = enabled ? readMmio(targetRegister) : 0U;

	if (enabled) {
		writeMmio(priorityRegister, (readMmio(priorityRegister) & ~(0xFFU << targetShift)) | (0xA0U << targetShift));
		writeMmio(targetRegister, (readMmio(targetRegister) & ~(0xFFU << targetShift)) | (1U << targetShift));
		writeMmio(kGicDistributor + 0x100U + bankOffset, bit); // ISENABLER
	}

	dsb();

	if (enabled && (readMmio(kGicDistributor + 0x100U + bankOffset) & bit) == 0U) {
		// Group0/Secure ownership cannot be inferred, but ignored writes
		// MUST NOT be reported as successful IRQ registration.
		writeMmio(priorityRegister, previousPriority);
		writeMmio(targetRegister, previousTarget);
		dsb();
		return false;
	}

	claimed = enabled;

	return true;
#endif
}

extern "C" void rk3506_exception_fatal(uint32_t type, uint32_t pc, uint32_t savedCpsr) {
	asm volatile("cpsid if" ::: "memory");
	rk3506_console_write("\nARM exception ");
	rk3506_console_decimal(type);
	rk3506_console_write(" at PC=");
	rk3506_console_hex(pc);
	rk3506_console_write(" SPSR=");
	rk3506_console_hex(savedCpsr);

	if (type == 4U) {
		// Cortex-A7 data-abort fault registers. DFSR identifies the reason;
		// DFAR gives the data address for a precise data abort.
		uint32_t dfsr;
		uint32_t dfar;
		asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(dfsr));
		asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));
		rk3506_console_write(" DFSR=");
		rk3506_console_hex(dfsr);
		rk3506_console_write(" DFAR=");
		rk3506_console_hex(dfar);
	}

	rk3506_console_write("; halted\n");
	while (true) {
		asm volatile("wfi");
	}
}

// Called only by `debug ram prepare`, from the running SVC shell task.
// Never returns: GDB must now halt CPU0, load ELF and start from _start.
extern "C" [[noreturn]] void rk3506_debug_park_for_ram_reload() {
	asm volatile("cpsid if" ::: "memory");
	setVirtualTimerControl(0U); // Stop the old OS' periodic source.
	// Mask/clear only the 1 ms PPI that this kernel owns. Do not reset GIC.
	writeMmio(kGicDistributor + 0x180U, 1U << kVirtualTimerPpi);
	writeMmio(kGicDistributor + 0x280U, 1U << kVirtualTimerPpi);
	dsb();
	isb();
	for (;;) {
		asm volatile("wfe" ::: "memory");
	}
}

extern "C" uint32_t rk3506_timer_ticks() {
	return g_ticks;
}

#if defined(LEXXOS_RK3506_FREERTOS)
namespace lexxos::platform::rk3506 {

bool armTickEventProbe(kernel::Event& event) noexcept {
	if (!event.valid() || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
		return false;
	}

	taskENTER_CRITICAL();
	bool const armed = g_tickProbeEvent == nullptr;
	if (armed) {
		g_tickProbeNotifications = 0;
		g_tickProbeEvent = &event;
	}

	taskEXIT_CRITICAL();
	return armed;
}

std::uint32_t disarmTickEventProbe(kernel::Event& event) noexcept {
	taskENTER_CRITICAL();
	std::uint32_t count = 0;
	if (g_tickProbeEvent == &event) {
		g_tickProbeEvent = nullptr;
		count = g_tickProbeNotifications;
		g_tickProbeNotifications = 0;
	}

	taskEXIT_CRITICAL();
	return count;
}

} // namespace lexxos::platform::rk3506

extern "C" void rk3506_freertos_setup_tick() {
	setVirtualTimerValue(g_tickReload);
	setVirtualTimerControl(1U);
}

extern "C" void rk3506_freertos_clear_tick() {
	// ARM virtual timer is level-triggered: re-arm before GICC_EOIR.
	setVirtualTimerValue(g_tickReload);
	g_ticks = g_ticks + 1U;

	if (auto* const event = g_tickProbeEvent; event != nullptr) {
		bool higherPriorityTaskWoken = false;
		if (event->signalFromIsr(higherPriorityTaskWoken)) {
			++g_tickProbeNotifications;
		}

		// FreeRTOS_Tick_Handler owns scheduling and the subsequent IRQ EOI.
		// Do not yield separately or alter the GIC configuration here.
	}
}
#endif

extern "C" uint64_t rk3506_timer_counter() {
	return readVirtualCounter();
}

extern "C" uint32_t rk3506_timer_counter_frequency() {
	return readCounterFrequency();
}

extern "C" uint32_t rk3506_timer_control() {
	return readVirtualTimerControl();
}

extern "C" uint32_t rk3506_timer_last_unexpected_irq() {
	return g_unexpectedIrq;
}

extern "C" bool rk3506_irq_timer_initialize() {
	// U-Boot/OP-TEE already initialized GIC security grouping and shared IRQs.
	// Do not reset GICD or disable other lines: only configure PPI 27.
	uint32_t const hz = readCounterFrequency();
	if (hz < 1000U || hz > 1000000000U) {
		rk3506_console_write("Generic Timer: invalid CNTFRQ=");
		rk3506_console_decimal(hz);
		rk3506_console_write("\n");
		return false;
	}

	g_tickReload = hz / 1000U;
	if (hz != kExpectedTimerHz) {
		rk3506_console_write("Generic Timer: unexpected clock (using CNTFRQ)\n");
	}

	rk3506_console_write("Generic Timer: ");
	rk3506_console_decimal(hz);
	rk3506_console_write(" Hz, PPI 27, tick 1 ms\n");

	// Mask the timer until GIC and vectors are ready.
	setVirtualTimerControl(0U);

	uint32_t const typer = readMmio(kGicDistributor + kGicdTyper);
	uint32_t const lineCount = ((typer & 31U) + 1U) * 32U;
	rk3506_console_write("GIC: ");
	rk3506_console_decimal(lineCount);
	rk3506_console_write(" interrupt IDs\n");
	if (lineCount <= kVirtualTimerPpi) {
		return false;
	}

#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	// Acquire the non-secure PPI before touching its priority. The manager
	// rejects Group0 or an already enabled firmware-owned line.
	lexxos::kernel::IrqRequest timerRequest{
	    .source = lexxos::platform::rk3506::GicIrqController::timerSource,
	    .owner = &g_timerOwner,
	    .handler = managedTimerInterrupt,
	};
	auto const attachStatus = g_irqManager.subscribe(timerRequest, g_timerSubscription);
	if (attachStatus != lexxos::kernel::IrqStatus::Ok) {
		rk3506_console_write("Generic Timer: managed IRQ27 attach refused, status=");
		rk3506_console_decimal(static_cast<uint32_t>(attachStatus));
		rk3506_console_write("\n");
		return false;
	}
#endif

	// Priority of PPI 27: byte 3 of the 32-bit GICD_IPRIORITYR6 word.
	// Keep other three priority bytes untouched.
	uintptr_t const priorityRegister = kGicDistributor + kGicdIpriorityr + (kVirtualTimerPpi & ~3U);
	uint32_t const shift = (kVirtualTimerPpi & 3U) * 8U;
	uint32_t const priority = readMmio(priorityRegister);

#if defined(LEXXOS_RK3506_FREERTOS)
	// Probe only the priority register of PPI27, which LexxOS owns.
	// The upstream ARM_CA9 port probes SPI32 instead; that is unsafe here.
	writeMmio(priorityRegister, (priority & ~(0xFFU << shift)) | (0xFFU << shift));
	uint32_t const supportedBits = (readMmio(priorityRegister) >> shift) & 0xFFU;
	writeMmio(priorityRegister, priority);

	if (supportedBits != 0xF0U) { // 16 unique priority levels.
		rk3506_console_write("FreeRTOS: GIC priority bits mismatch, read=");
		rk3506_console_hex(supportedBits);
		rk3506_console_write("\n");
		return false;
	}
#endif

	writeMmio(priorityRegister, (priority & ~(0xFFU << shift)) | (0xA0U << shift));

#if !defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	writeMmio(kGicDistributor + kGicdIsenabler0, 1U << kVirtualTimerPpi);
#endif

	writeMmio(kGicDistributor + kGicdCtlr, readMmio(kGicDistributor + kGicdCtlr) | 1U);
	writeMmio(kGicCpuInterface + kGiccPmr, 0xFFU);
	writeMmio(kGicCpuInterface + kGiccCtlr, readMmio(kGicCpuInterface + kGiccCtlr) | 1U);

#if defined(LEXXOS_RK3506_FREERTOS)
	// The ARM_CA9 port requires all implemented priority bits to preempt.
	writeMmio(kGicCpuInterface + 0x008U, 0U); // GICC_BPR
	if ((readMmio(kGicCpuInterface + 0x008U) & 0x7U) > 3U) {
		rk3506_console_write("FreeRTOS: unsupported GIC binary point\n");
		return false;
	}
#endif

	dsb();

	g_ticks = 0U;
	g_unexpectedIrq = 0U;

#if defined(LEXXOS_RK3506_FREERTOS)
	// Never deliver a tick before xPortStartScheduler installs the RTOS context.
	rk3506_console_write("GIC / IRQ: FreeRTOS tick deferred until scheduler start\n");
	return true;
#else

	setVirtualTimerValue(g_tickReload);

	#if defined(LEXXOS_RK3506_MANAGED_TIMER_IRQ)
	auto const enableStatus = g_irqManager.setEnabled(g_timerSubscription, &g_timerOwner, true);
	if (enableStatus != lexxos::kernel::IrqStatus::Ok) {
		rk3506_console_write("Generic Timer: managed IRQ27 enable failed\n");
		return false;
	}
	#endif

	setVirtualTimerControl(1U);
	asm volatile("cpsie i" ::: "memory");
	isb();
	rk3506_console_write("GIC / IRQ: enabled\n");
	return true;
#endif
}

extern "C" void rk3506_irq_timer_check(uint32_t const timeoutMs) {
	uint32_t const startTicks = g_ticks;
	uint32_t const hz = readCounterFrequency();
	uint64_t const counterAtStart = readVirtualCounter();
	uint64_t const cycleLimit = static_cast<uint64_t>(hz / 1000U) * timeoutMs;
	// The spin limit prevents indefinite blocking if the timer is gated.
	for (uint32_t spins = 0U; spins < 50000000U && g_ticks == startTicks; ++spins) {
		if (readVirtualCounter() - counterAtStart >= cycleLimit) {
			break;
		}

		asm volatile("nop");
	}

	if (g_ticks == startTicks) {
		rk3506_console_write("ERROR: no timer IRQ observed; CNTV_CTL=");
		rk3506_console_hex(readVirtualTimerControl());
		rk3506_console_write(" GICC_CTLR=");
		rk3506_console_hex(readMmio(kGicCpuInterface + kGiccCtlr));
		rk3506_console_write(" GICD_CTLR=");
		rk3506_console_hex(readMmio(kGicDistributor + kGicdCtlr));
		rk3506_console_write(" PPI27 group=unknown (Non-secure) enabled=");
		rk3506_console_decimal((readMmio(kGicDistributor + kGicdIsenabler0) >> kVirtualTimerPpi) & 1U);
		rk3506_console_write("\n");
	} else {
		rk3506_console_write("IRQ self-test: OK\n");
	}
}
