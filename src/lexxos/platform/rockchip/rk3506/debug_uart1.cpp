/* RK3506 debugging peripherals: UART1 via RM_IO10/11 and Cortex-A7 SWD.
 * This code only runs when explicitly selected by the RK3506 CMake options.
 * U-Boot/DDR/OP-TEE are not reset or reconfigured here.
 */
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::uintptr_t kUart1 = 0xFF0B0000U;
constexpr std::uintptr_t kCru = 0xFF9A0000U;
constexpr std::uintptr_t kRmio = 0xFF910000U;
constexpr std::uintptr_t kGpio0Ioc = 0xFF950000U;
constexpr std::uintptr_t kGrf = 0xFF288000U;

// 16550/DW-UART, 32-bit MMIO with register shift 2.
constexpr std::uintptr_t kRbrThrDll = 0x00U;
constexpr std::uintptr_t kIerDlm = 0x04U;
constexpr std::uintptr_t kFcr = 0x08U;
constexpr std::uintptr_t kLcr = 0x0CU;
constexpr std::uintptr_t kLsr = 0x14U;
constexpr std::uint32_t kRxReady = 1U << 0U;
constexpr std::uint32_t kTxEmpty = 1U << 5U;

std::uint32_t read32(std::uintptr_t address) {
	return *reinterpret_cast<std::uint32_t const volatile*>(address);
}

void write32(std::uintptr_t address, std::uint32_t value) {
	*reinterpret_cast<std::uint32_t volatile*>(address) = value;
}

void assignRmio(unsigned pin, std::uint32_t function) {
	// 7-bit per-pin function; high half-word is the write mask.
	write32(kRmio + 0x80U + pin * 4U, (0x7FU << 16U) | function);
}

} // namespace

extern "C" bool rk3506_debug_uart1_initialize() {
	// CLKSEL_CON30: UART1 parent = 24 MHz XTAL (mux 0), divider = 1.
	// Change only bits [7:0] to preserve UART2 on bits [15:8].
	write32(kCru + 0x378U, 0x00FF0000U);
	// CLKGATE_CON11: ungate PCLK_UART1 (bit 5), SCLK_UART1 (bit 10).
	write32(kCru + 0x82CU, (1U << 21U) | (1U << 26U));

	// UART1: 8N1, 115384 baud from 24 MHz / (16 * 13).
	write32(kUart1 + kIerDlm, 0U); // Disable interrupts, DLAB = 0.
	write32(kUart1 + kLcr, 0x83U); // DLAB = 1, 8N1.
	write32(kUart1 + kRbrThrDll, 1U); // DLL.
	write32(kUart1 + kIerDlm, 0U); // DLM.
	write32(kUart1 + kLcr, 0x03U); // DLAB = 0, 8N1.
	write32(kUart1 + kFcr, 0x07U); // FIFO on, clear RX/TX.
	if ((read32(kUart1 + kLcr) & 0xFFU) != 0x03U) {
		return false;
	}

	// GPIO0_B2/B3 are RM_IO10/11; function 7 selects RMIO on each pin.
	// GPIO0_B[3:0] IOMUX register at IOC + 0x08; preserve B0/B1.
	assignRmio(10U, 1U); // UART1_TX -> GPIO0_B2, Lyra header pin 8.
	assignRmio(11U, 2U); // UART1_RX -> GPIO0_B3, Lyra header pin 9.
	write32(kGpio0Ioc + 0x08U, 0xFF007700U);
	asm volatile("dsb sy; isb sy" ::: "memory");
	return true;
}

extern "C" void rk3506_debug_uart1_putc(char character) {
	for (std::uint32_t attempt = 0; attempt < 1000000U; ++attempt) {
		if (read32(kUart1 + kLsr) & kTxEmpty) {
			write32(kUart1 + kRbrThrDll,
			    static_cast<std::uint32_t>(static_cast<unsigned char>(character)));
			return;
		}
	}
}

extern "C" std::size_t rk3506_debug_uart1_read_available(char* buffer, std::size_t capacity) {
	if (buffer == nullptr) {
		return 0U;
	}

	std::size_t count = 0;
	while (count < capacity && (read32(kUart1 + kLsr) & kRxReady)) {
		buffer[count++] = static_cast<char>(read32(kUart1 + kRbrThrDll) & 0xFFU);
	}

	return count;
}

#if defined(LEXXOS_RK3506_FREERTOS)
extern "C" void rk3506_debug_uart1_set_rx_interrupt(bool enabled) {
	constexpr std::uint32_t receive = 1U;
	if (enabled) {
		write32(kUart1 + kFcr, 0x41U); // FIFO enabled, RX trigger at one quarter, no reset.
	}
	auto const ier = read32(kUart1 + kIerDlm);
	write32(kUart1 + kIerDlm, enabled ? ier | receive : ier & ~receive);
}
#endif

// Detect the *hardware* pinmux, not an OS variable erased by RAM re-entry.
// C6/C7 are SWD only when both corresponding IOC nibbles select function 2.
extern "C" bool rk3506_debug_swd_pins_active() {
	return (read32(kGpio0Ioc + 0x14U) & 0x0000FF00U) == 0x00002200U;
}

extern "C" void rk3506_debug_enable_a7_swd() {
	// GRF_SOC_CON0[5:4] = 0: choose Cortex-A7 JTAG/SWD, NOT M0.
	write32(kGrf, 0x00300000U);
	// GPIO0_C6/C7 -> SWCLK/SWDIO (function 2), overriding UART0.
	// GPIO0C_IOMUX_SEL_1: C6 shift8, C7 shift12; high half is mask.
	write32(kGpio0Ioc + 0x14U, 0xFF002200U);
	asm volatile("dsb sy; isb sy" ::: "memory");
}
