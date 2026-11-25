#include <lexxos/devices/bus/spi_transfer.h>

namespace lexxos::devices {

SpiTransferStatus queuedSpiTransfer(SpiBus& bus, SpiClockMode mode, std::uint32_t baud,
    std::uint8_t const* tx, std::uint8_t* rx, std::size_t size,
    DeviceResourcePriority priority, std::uint32_t timeoutMs) {
	SpiBusTransaction transaction{bus, priority, timeoutMs};
	if (!transaction) {
		return SpiTransferStatus::Busy;
	}

	if (!bus.setClockMode(mode) || (baud != 0 && !bus.setBaudRate(baud))) {
		return SpiTransferStatus::IoError;
	}

	bus.select();
	bool const success = tx != nullptr && rx != nullptr ? bus.transfer(tx, rx, size)
	    : tx != nullptr                                 ? bus.write(tx, size)
	                                                    : bus.read(0xFFU, rx, size);

	bus.deselect();
	return success ? SpiTransferStatus::Ok : SpiTransferStatus::IoError;
}

} // namespace lexxos::devices
