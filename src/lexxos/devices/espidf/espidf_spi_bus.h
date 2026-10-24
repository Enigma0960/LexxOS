#pragma once

#include <cstddef>
#include <cstdint>

#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

namespace espidf::detail {

	struct SpiHostState {
		bool initialized{false};
		lexxos::platform::SpiPins pins{};
	};

} // namespace espidf::detail

class EspIdfSpiBus final : public SpiBus {
public:
	~EspIdfSpiBus() override;

	bool initialize(lexxos::platform::SpiPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool setBaudRate(std::uint32_t baudRate) override;
	bool setClockMode(SpiClockMode mode) override;
	[[nodiscard]] SpiClockMode clockMode() const override;
	void select() override;
	void deselect() override;
	bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool write(std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) override;
	bool beginTransaction() override;
	void endTransaction() override;

private:
	[[nodiscard]] bool _lock();
	void _unlock();
	bool _configureDevice();
	bool _transmit(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size);
	bool _transmitLocked(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size);

	spi_host_device_t _host{SPI2_HOST};
	spi_device_handle_t _device{nullptr};
	lexxos::platform::SpiPins _pins{};
	SpiClockMode _mode{SpiClockMode::Mode0};
	SemaphoreHandle_t _mutex{nullptr};
	std::uint32_t _selectLockDepth{0};
	bool _ready{false};
};

} // namespace lexxos::devices
