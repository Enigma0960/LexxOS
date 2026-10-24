#include <lexxos/platform/pico/detail/pico_console.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/uart.h>

#if defined(LEXXOS_PICO_USB_CDC_CONSOLE) && LEXXOS_PICO_USB_CDC_CONSOLE
	#include <pico/stdio.h>
	#include <pico/stdio_usb.h>
#endif

#include <lexxos/platform/platform.h>

namespace {

constexpr std::size_t consoleUartRxBufferSize = 4096;
static_assert((consoleUartRxBufferSize & (consoleUartRxBufferSize - 1U)) == 0U, "console RX buffer size must be power of two");

std::array<char, consoleUartRxBufferSize> g_uartRxBuffer{};
std::uint16_t volatile g_uartRxHead = 0;
std::uint16_t volatile g_uartRxTail = 0;
std::uint32_t volatile g_uartRxDropped = 0;

uart_inst_t* consoleUart() {
	return lexxos::platform::activeBoardDescriptor().debugUart.index == 1 ? uart1 : uart0;
}

int consoleUartIrq() {
	return consoleUart() == uart1 ? UART1_IRQ : UART0_IRQ;
}

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}

std::uint16_t nextUartRxIndex(std::uint16_t value) {
	return static_cast<std::uint16_t>((value + 1U) & (consoleUartRxBufferSize - 1U));
}

void resetUartRxBuffer() {
	g_uartRxHead = 0;
	g_uartRxTail = 0;
	g_uartRxDropped = 0;
}

void pushUartRxByte(char value) {
	auto const head = g_uartRxHead;
	auto const next = nextUartRxIndex(head);
	if (next == g_uartRxTail) {
		g_uartRxDropped = g_uartRxDropped + 1U;
		return;
	}

	g_uartRxBuffer[head] = value;
	g_uartRxHead = next;
}

bool popUartRxByte(char& value) {
	auto const tail = g_uartRxTail;
	if (tail == g_uartRxHead) {
		return false;
	}

	value = g_uartRxBuffer[tail];
	g_uartRxTail = nextUartRxIndex(tail);
	return true;
}

void drainConsoleUartRx() {
	auto* uart = consoleUart();
	while (uart_is_readable(uart)) {
		pushUartRxByte(static_cast<char>(uart_getc(uart)));
	}
}

void consoleUartIrqHandler() {
	drainConsoleUartRx();
}

} // namespace

void lexxos::platform::pico::detail::initializeConsole() {
	auto const pins = effectiveDebugUart(activeBoardDescriptor());
	if (pins.isValid()) {
		uart_init(consoleUart(), pins.baudRate);
		gpio_set_function(toPicoGpioPin(pins.tx), GPIO_FUNC_UART);
		gpio_set_function(toPicoGpioPin(pins.rx), GPIO_FUNC_UART);
		resetUartRxBuffer();
		drainConsoleUartRx();
		irq_set_exclusive_handler(consoleUartIrq(), consoleUartIrqHandler);
		irq_set_enabled(consoleUartIrq(), true);
		uart_set_irq_enables(consoleUart(), true, false);
	}

#if defined(LEXXOS_PICO_USB_CDC_CONSOLE) && LEXXOS_PICO_USB_CDC_CONSOLE
	(void) stdio_usb_init();
#endif
}

void lexxos::platform::writeConsole(std::string_view text) {
	for (char const value : text) {
		uart_putc_raw(consoleUart(), value);
	}

#if defined(LEXXOS_PICO_USB_CDC_CONSOLE) && LEXXOS_PICO_USB_CDC_CONSOLE
	if (stdio_usb_connected()) {
		for (char const value : text) {
			(void) stdio_putchar_raw(value);
		}
	}
#endif
}

bool lexxos::platform::readConsoleChar(char& value) {
	return readConsoleAvailable(&value, 1) == 1;
}

std::size_t lexxos::platform::readConsoleAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

	std::size_t count = 0;
#if defined(LEXXOS_PICO_USB_CDC_CONSOLE) && LEXXOS_PICO_USB_CDC_CONSOLE
	while (count < capacity) {
		auto const usbValue = stdio_getchar_timeout_us(0);
		if (usbValue < 0) {
			break;
		}
		buffer[count++] = static_cast<char>(usbValue);
	}
#endif

	drainConsoleUartRx();
	while (count < capacity && popUartRxByte(buffer[count])) {
		++count;
	}
	return count;
}
