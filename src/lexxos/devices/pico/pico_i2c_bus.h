#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <hardware/i2c.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/pico/detail/i2c_bus_guard.h>
#include <lexxos/devices/pico/detail/i2c_bus_mutex.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices {

class PicoI2cBus final : public I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool probe(std::uint8_t address) override;
	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override;
	bool beginTransaction() override;
	void endTransaction() override;

private:
	void _configureI2cPins();
	void _recoverBus();

	lexxos::platform::I2cPins _pins{};
	i2c_inst_t* _instance{nullptr};
	I2cBusMutex _mutex;
	bool _ready{false};
};

} // namespace lexxos::devices
