#include <lexxos/platform/espidf/detail/espidf_console.h>

#include <algorithm>
#include <cstring>

#include <driver/uart.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <soc/soc_caps.h>

#include <lexxos/platform/platform.h>

#ifndef SOC_USB_SERIAL_JTAG_SUPPORTED
	#define SOC_USB_SERIAL_JTAG_SUPPORTED 0
#endif

#if SOC_USB_SERIAL_JTAG_SUPPORTED
	#include <driver/usb_serial_jtag.h>
#endif

namespace {

// One FreeRTOS tick is enough to let ESP-IDF idle tasks run and feed task WDT.
constexpr TickType_t consoleReadTimeoutTicks = 1;
constexpr int consoleBufferBytes = 4096;

bool g_uartConsoleReady{false};

#if SOC_USB_SERIAL_JTAG_SUPPORTED
bool g_usbSerialJtagConsoleReady{false};
#endif

uart_port_t consoleUart() {
	switch (lexxos::platform::activeBoardDescriptor().debugUart.index) {
		case 1:
			return UART_NUM_1;
		case 2:
			return UART_NUM_2;
		case 0:
		default:
			return UART_NUM_0;
	}
}

int toEspGpio(lexxos::platform::GpioPin pin) {
	return pin.isValid() ? pin.number : UART_PIN_NO_CHANGE;
}

void initializeUartConsole() {
	auto const& pins = lexxos::platform::activeBoardDescriptor().debugUart;
	if (!pins.isValid()) {
		return;
	}

	uart_config_t config{};
	config.baud_rate = static_cast<int>(pins.baudRate);
	config.data_bits = UART_DATA_8_BITS;
	config.parity = UART_PARITY_DISABLE;
	config.stop_bits = UART_STOP_BITS_1;
	config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	config.source_clk = UART_SCLK_DEFAULT;

	auto const uart = consoleUart();
	if (uart_param_config(uart, &config) != ESP_OK) {
		return;
	}
	if (uart_set_pin(uart, toEspGpio(pins.tx), toEspGpio(pins.rx), UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
		return;
	}
	if (uart_driver_install(uart, consoleBufferBytes, 0, 0, nullptr, 0) != ESP_OK) {
		return;
	}
	g_uartConsoleReady = true;
}

void initializeUsbSerialJtagConsole() {
#if SOC_USB_SERIAL_JTAG_SUPPORTED
	usb_serial_jtag_driver_config_t config{};
	config.tx_buffer_size = consoleBufferBytes;
	config.rx_buffer_size = consoleBufferBytes;
	auto const result = usb_serial_jtag_driver_install(&config);
	g_usbSerialJtagConsoleReady = result == ESP_OK || result == ESP_ERR_INVALID_STATE;
#endif
}

} // namespace

void lexxos::platform::espidf::detail::initializeConsole() {
	initializeUsbSerialJtagConsole();
	initializeUartConsole();
}

void lexxos::platform::writeConsole(std::string_view text) {
	if (text.empty()) {
		return;
	}

#if SOC_USB_SERIAL_JTAG_SUPPORTED
	if (g_usbSerialJtagConsoleReady) {
		usb_serial_jtag_write_bytes(text.data(), text.size(), 0);
	}
#endif

	if (g_uartConsoleReady) {
		uart_write_bytes(consoleUart(), text.data(), text.size());
	}
}

bool lexxos::platform::readConsoleChar(char& value) {
	std::uint8_t byte{0};

#if SOC_USB_SERIAL_JTAG_SUPPORTED
	if (g_usbSerialJtagConsoleReady) {
		auto const usbBytesRead = usb_serial_jtag_read_bytes(&byte, 1, consoleReadTimeoutTicks);
		if (usbBytesRead == 1) {
			value = static_cast<char>(byte);
			return true;
		}
	}
#endif

	if (!g_uartConsoleReady) {
		return false;
	}

	auto const bytesRead = uart_read_bytes(consoleUart(), &byte, 1, consoleReadTimeoutTicks);
	if (bytesRead != 1) {
		return false;
	}

	value = static_cast<char>(byte);
	return true;
}

std::size_t lexxos::platform::readConsoleAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

	std::size_t count = 0;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
	if (g_usbSerialJtagConsoleReady) {
		auto const usbBytesRead = usb_serial_jtag_read_bytes(buffer, capacity, 0);
		if (usbBytesRead > 0) {
			count += static_cast<std::size_t>(usbBytesRead);
		}
	}
#endif

	if (count >= capacity || !g_uartConsoleReady) {
		return count;
	}

	auto const uartBytesRead = uart_read_bytes(
	    consoleUart(),
	    reinterpret_cast<std::uint8_t*>(buffer + count),
	    capacity - count,
	    0);
	if (uartBytesRead > 0) {
		count += static_cast<std::size_t>(uartBytesRead);
	}
	return count;
}
