#pragma once

#include <lexx/lexx.h>

namespace lexx {

enum class DeviceKind : uint32_t {
	Gpio = 1,
	I2c = 2,
	Spi = 3,
	Uart = 4,
};

enum class DeviceStatus : int {
	Ok = 0,
	Invalid = -1,
	NotFound = -2,
	Busy = -3,
	PermissionDenied = -4,
	NotReady = -5,
	Unsupported = -6,
	IoError = -7,
	NoHandles = -8,
};

inline constexpr uint32_t device_flag_ready = 1U;
inline constexpr uint32_t device_flag_busy = 2U;
inline constexpr uint32_t device_flag_input = 4U;
inline constexpr uint32_t device_flag_output = 8U;
inline constexpr uint32_t device_max_transfer = 256U;

struct DeviceInfo {
	uint32_t kind{0};
	uint32_t flags{0};
	char name[32]{};
};

struct DeviceRequest {
	uint32_t operation{0};
	uint32_t flags{0};
	uint32_t arg0{0};
	uint32_t arg1{0};
	uint32_t arg2{0};
	uint32_t arg3{0};
	uint32_t result{0};
};

static_assert(sizeof(DeviceInfo) == 40);
static_assert(sizeof(DeviceRequest) == 28);

namespace device_operation {

	inline constexpr uint32_t gpio_input = 1;
	inline constexpr uint32_t gpio_output = 2;
	inline constexpr uint32_t gpio_read = 3;
	inline constexpr uint32_t gpio_write = 4;

	inline constexpr uint32_t i2c_probe = 10;
	inline constexpr uint32_t i2c_write = 11;
	inline constexpr uint32_t i2c_read = 12;

	inline constexpr uint32_t spi_transfer = 20;

	inline constexpr uint32_t uart_configure = 30;
	inline constexpr uint32_t uart_write = 31;
	inline constexpr uint32_t uart_read = 32;

} // namespace lexx::device_operation

inline int device_info(DeviceKind kind, uint32_t index,
    DeviceInfo& info) {
	return static_cast<int>(detail::ecall3(
	    syscall_device_info,
	    static_cast<uint32_t>(kind),
	    index,
	    reinterpret_cast<uint32_t>(&info)));
}

inline int device_open(DeviceKind kind, char const* name) {
	return static_cast<int>(detail::ecall3(
	    syscall_device_open,
	    static_cast<uint32_t>(kind),
	    reinterpret_cast<uint32_t>(name), 0));
}

inline int device_close(int handle) {
	return static_cast<int>(detail::ecall1(
	    syscall_device_close, static_cast<uint32_t>(handle)));
}

inline int device_control(int handle, DeviceRequest& request) {
	return static_cast<int>(detail::ecall3(
	    syscall_device_control,
	    static_cast<uint32_t>(handle),
	    reinterpret_cast<uint32_t>(&request), 0));
}

inline int gpio_configure_input(int handle, uint32_t pull = 0) {
	DeviceRequest request{};
	request.operation = device_operation::gpio_input;
	request.flags = pull;
	return device_control(handle, request);
}

inline int gpio_configure_output(int handle, bool initial = false) {
	DeviceRequest request{};
	request.operation = device_operation::gpio_output;
	request.arg0 = initial ? 1U : 0U;
	return device_control(handle, request);
}

inline int gpio_read(int handle, bool& value) {
	DeviceRequest request{};
	request.operation = device_operation::gpio_read;
	auto const status = device_control(handle, request);
	if (status == 0) {
		value = request.result != 0;
	}

	return status;
}

inline int gpio_write(int handle, bool value) {
	DeviceRequest request{};
	request.operation = device_operation::gpio_write;
	request.arg0 = value ? 1U : 0U;
	return device_control(handle, request);
}

inline int i2c_probe(int handle, uint8_t address, bool& found) {
	DeviceRequest request{};
	request.operation = device_operation::i2c_probe;
	request.arg0 = address;
	auto const status = device_control(handle, request);
	if (status == 0) {
		found = request.result != 0;
	}

	return status;
}

inline int i2c_write(int handle, uint8_t address,
    void const* data, size_t size) {
	DeviceRequest request{};
	request.operation = device_operation::i2c_write;
	request.arg0 = address;
	request.arg1 = reinterpret_cast<uint32_t>(data);
	request.arg2 = static_cast<uint32_t>(size);
	return device_control(handle, request);
}

inline int i2c_read(int handle, uint8_t address,
    void* data, size_t size) {
	DeviceRequest request{};
	request.operation = device_operation::i2c_read;
	request.arg0 = address;
	request.arg1 = reinterpret_cast<uint32_t>(data);
	request.arg2 = static_cast<uint32_t>(size);
	return device_control(handle, request);
}

inline int spi_transfer(int handle, void const* tx, void* rx,
    size_t size, uint32_t baud = 0,
    uint32_t mode = 0) {
	DeviceRequest request{};
	request.operation = device_operation::spi_transfer;
	request.flags = mode;
	request.arg0 = reinterpret_cast<uint32_t>(tx);
	request.arg1 = reinterpret_cast<uint32_t>(rx);
	request.arg2 = static_cast<uint32_t>(size);
	request.arg3 = baud;
	return device_control(handle, request);
}

inline int uart_configure(int handle, uint32_t baud) {
	DeviceRequest request{};
	request.operation = device_operation::uart_configure;
	request.arg0 = baud;
	return device_control(handle, request);
}

inline int uart_write(int handle, void const* data, size_t size,
    size_t& written) {
	DeviceRequest request{};
	request.operation = device_operation::uart_write;
	request.arg0 = reinterpret_cast<uint32_t>(data);
	request.arg1 = static_cast<uint32_t>(size);
	auto const status = device_control(handle, request);
	if (status == 0) {
		written = request.result;
	}

	return status;
}

inline int uart_read(int handle, void* data, size_t capacity,
    size_t& received) {
	DeviceRequest request{};
	request.operation = device_operation::uart_read;
	request.arg0 = reinterpret_cast<uint32_t>(data);
	request.arg1 = static_cast<uint32_t>(capacity);
	auto const status = device_control(handle, request);
	if (status == 0) {
		received = request.result;
	}

	return status;
}

} // namespace lexx
