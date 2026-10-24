#include <lexxos/platform/stm32/detail/stm32_console.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/platform/stm32/detail/stm32_gpio.h>

namespace lexxos::platform::stm32::detail {
namespace {

	constexpr std::uint32_t peripheralBase = 0x40000000U;
	constexpr std::uint32_t ahb1Base = peripheralBase + 0x00020000U;
	constexpr std::uint32_t apb2Base = peripheralBase + 0x00010000U;
	constexpr std::uint32_t rccBase = ahb1Base + 0x3800U;
	constexpr std::uint32_t usart1Base = apb2Base + 0x1000U;
	constexpr std::uint32_t nvicIserBase = 0xE000E100U;
	constexpr std::uint32_t nvicIprBase = 0xE000E400U;

#define REG32(address) (*reinterpret_cast<volatile std::uint32_t*>(address))
#define REG8(address) (*reinterpret_cast<volatile std::uint8_t*>(address))

#define RCC_APB2ENR REG32(rccBase + 0x44U)

#define USART_SR REG32(usart1Base + 0x00U)
#define USART_DR REG32(usart1Base + 0x04U)
#define USART_BRR REG32(usart1Base + 0x08U)
#define USART_CR1 REG32(usart1Base + 0x0CU)
#define USART_CR2 REG32(usart1Base + 0x10U)
#define USART_CR3 REG32(usart1Base + 0x14U)

#define NVIC_ISER(index) REG32(nvicIserBase + (static_cast<std::uint32_t>(index) * 4U))
#define NVIC_IPR(irqNumber) REG8(nvicIprBase + static_cast<std::uint32_t>(irqNumber))

	constexpr std::uint32_t rccApb2enrUsart1en = 1U << 4U;
	constexpr std::uint32_t usartSrPe = 1U << 0U;
	constexpr std::uint32_t usartSrFe = 1U << 1U;
	constexpr std::uint32_t usartSrNe = 1U << 2U;
	constexpr std::uint32_t usartSrOre = 1U << 3U;
	constexpr std::uint32_t usartSrRxne = 1U << 5U;
	constexpr std::uint32_t usartSrTxe = 1U << 7U;
	constexpr std::uint32_t usartCr1Re = 1U << 2U;
	constexpr std::uint32_t usartCr1Te = 1U << 3U;
	constexpr std::uint32_t usartCr1Rxneie = 1U << 5U;
	constexpr std::uint32_t usartCr1Ue = 1U << 13U;
	constexpr std::uint32_t usartStatusRxErrorMask = usartSrPe | usartSrFe | usartSrNe | usartSrOre;
	constexpr std::uint32_t apb2ClockHz = 84000000U;
	constexpr std::uint32_t defaultBaudRate = 115200U;
	constexpr std::int32_t usart1IrqNumber = 37;
	constexpr std::uint8_t usart1IrqPriority = 0xF0U;
	constexpr std::size_t consoleRxBufferSize = 4096;
	static_assert((consoleRxBufferSize & (consoleRxBufferSize - 1U)) == 0U, "console RX buffer size must be power of two");

	std::array<char, consoleRxBufferSize> g_consoleRxBuffer{};
	std::uint16_t volatile g_consoleRxHead = 0;
	std::uint16_t volatile g_consoleRxTail = 0;
	std::uint32_t volatile g_consoleRxDropped = 0;
	std::uint32_t volatile g_consoleRxErrors = 0;

	std::uint16_t nextConsoleRxIndex(std::uint16_t value) {
		return static_cast<std::uint16_t>((value + 1U) & (consoleRxBufferSize - 1U));
	}

	void resetConsoleRxBuffer() {
		g_consoleRxHead = 0;
		g_consoleRxTail = 0;
		g_consoleRxDropped = 0;
		g_consoleRxErrors = 0;
	}

	void pushConsoleRxByte(char value) {
		auto const head = g_consoleRxHead;
		auto const next = nextConsoleRxIndex(head);
		if (next == g_consoleRxTail) {
			g_consoleRxDropped = g_consoleRxDropped + 1U;
			return;
		}

		g_consoleRxBuffer[head] = value;
		g_consoleRxHead = next;
	}

	bool popConsoleRxByte(char& value) {
		auto const tail = g_consoleRxTail;
		if (tail == g_consoleRxHead) {
			return false;
		}

		value = g_consoleRxBuffer[tail];
		g_consoleRxTail = nextConsoleRxIndex(tail);
		return true;
	}

	void enableIrq(std::int32_t irqNumber, std::uint8_t priority) {
		if (irqNumber < 0) {
			return;
		}

		NVIC_IPR(irqNumber) = priority;
		NVIC_ISER(static_cast<std::uint32_t>(irqNumber) / 32U) = 1U << (static_cast<std::uint32_t>(irqNumber) % 32U);
	}

	void writeUartChar(char value) {
		while ((USART_SR & usartSrTxe) == 0U) {
		}
		USART_DR = static_cast<std::uint32_t>(static_cast<unsigned char>(value));
	}

} // namespace

void initializeConsole(UartPins const& pins) {
	if (!pins.isValid()) {
		return;
	}

	// Сначала переводим выводы USART в альтернативную функцию, затем включаем сам периферийный блок.
	if (!configureAlternateFunction(pins.tx, 7U, GpioPull::None, 2U)
	    || !configureAlternateFunction(pins.rx, 7U, GpioPull::Up, 2U)) {
		return;
	}

	RCC_APB2ENR |= rccApb2enrUsart1en;
	(void) RCC_APB2ENR;

	// Очищаем старое состояние до включения приёма и IRQ, чтобы boot начинался с пустой очереди.
	USART_CR1 = 0U;
	USART_CR2 = 0U;
	USART_CR3 = 0U;
	USART_BRR = apb2ClockHz / (pins.baudRate == 0 ? defaultBaudRate : pins.baudRate);
	resetConsoleRxBuffer();
	(void) USART_SR;
	(void) USART_DR;
	enableIrq(usart1IrqNumber, usart1IrqPriority);
	USART_CR1 = usartCr1Te | usartCr1Re | usartCr1Rxneie | usartCr1Ue;
}

void writeConsole(std::string_view text) {
	for (char const value : text) {
		writeUartChar(value);
	}
}

bool readConsoleChar(char& value) {
	return popConsoleRxByte(value);
}

std::size_t readConsoleAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

	std::size_t count = 0;
	while (count < capacity && popConsoleRxByte(buffer[count])) {
		++count;
	}
	return count;
}

extern "C" void USART1_IRQHandler(void) {
	for (std::uint32_t guard = 0; guard < consoleRxBufferSize; ++guard) {
		auto const status = USART_SR;
		if ((status & (usartSrRxne | usartStatusRxErrorMask)) == 0U) {
			return;
		}
		auto const data = static_cast<char>(USART_DR & 0xFFU);
		if ((status & usartStatusRxErrorMask) != 0U) {
			g_consoleRxErrors = g_consoleRxErrors + 1U;
		}
		if ((status & usartSrRxne) != 0U) {
			pushConsoleRxByte(data);
		}
	}
}

extern "C" void lexxos_stm32_uart_write_raw(char const* data, std::size_t size) {
	if (data == nullptr) {
		return;
	}
	writeConsole({data, size});
}

} // namespace lexxos::platform::stm32::detail
