#pragma once

#include <cstdint>

#include <lexxos/devices/device_resource.h>

namespace lexxos::devices {

class I2cBus;

class I2cBusTransaction {
public:
	explicit I2cBusTransaction(I2cBus& bus,
	    DeviceResourcePriority priority = DeviceResourcePriority::System,
	    std::uint32_t timeoutMs = 100);

	I2cBusTransaction(I2cBusTransaction const&) = delete;
	I2cBusTransaction& operator=(I2cBusTransaction const&) = delete;

	I2cBusTransaction(I2cBusTransaction&& other) noexcept;

	I2cBusTransaction& operator=(I2cBusTransaction&& other) noexcept;

	~I2cBusTransaction();

	[[nodiscard]] bool active() const;

	[[nodiscard]] explicit operator bool() const;

	void release();

private:
	I2cBus* _bus{nullptr};
	bool _active{false};
};

} // namespace lexxos::devices
