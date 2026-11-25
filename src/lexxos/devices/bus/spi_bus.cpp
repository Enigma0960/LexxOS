#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

SpiBus::SpiBus(BusTransactionQueue& queue)
    : _queue(&queue) {
}

bool SpiBus::beginTransaction() {
	return true;
}

void SpiBus::endTransaction() {
}

bool SpiBus::supportsDma() const {
	return false;
}

bool SpiBus::startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (tx != nullptr && rx != nullptr) {
		return transfer(tx, rx, size);
	}

	if (tx != nullptr) {
		return write(tx, size);
	}

	if (rx != nullptr) {
		return read(0xFF, rx, size);
	}

	return size == 0;
}

bool SpiBus::waitDmaTransfer(std::uint32_t timeoutMs) {
	(void) timeoutMs;
	return true;
}

bool SpiBus::transferDma(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size, std::uint32_t timeoutMs) {
	return startDmaTransfer(tx, rx, size) && waitDmaTransfer(timeoutMs);
}

} // namespace lexxos::devices
