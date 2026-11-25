#pragma once

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

enum class SpiTransferStatus {
	Ok,
	Busy,
	IoError,
};

// Синхронная операция: настройка и весь интервал CS под одной транзакцией.
SpiTransferStatus queuedSpiTransfer(SpiBus& bus, SpiClockMode mode, std::uint32_t baud,
    std::uint8_t const* tx, std::uint8_t* rx, std::size_t size,
    DeviceResourcePriority priority = DeviceResourcePriority::Normal, std::uint32_t timeoutMs = 50);

} // namespace lexxos::devices
